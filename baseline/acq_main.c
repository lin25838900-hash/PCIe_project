/*
 * PCIe Acquisition Card Driver - Baseline Version (modularized)
 */

#include "pcie_acq_baseline.h"

struct acq_device *g_acq_dev;

static int acq_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct acq_device *dev;
    int ret, i;

    //dev_info:表示进入到probe阶段 也就是pci系统枚举到了pci设备
    dev_info(&pdev->dev, "PCIe acquisition card detected\n");

    //初始化自定义的设备私有数据结构，保存后续的所有状态 用到的是kzalloc(size,flag)
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
        //绑定到私有的设备数据结构里面
    dev->pdev = pdev;
    pci_set_drvdata(pdev, dev);//设备私有数据的绑定函数作用是将驱动自定义的数据结构体与内核管理的pci设备结构体进行永久绑定，本质上就是给pdev附加了一个驱动的专属扩展属性

    g_acq_dev = dev;

    init_waitqueue_head(&dev->wait_queue);//probe阶段初始化内核等待队列 实现内核托管机制
    spin_lock_init(&dev->dma_lock);
    init_completion(&dev->dma_done);
    dev->dma_busy = false;
    dev->last_dma_status = 0;
    
    //上述是初始化自定义驱动结构，下面就是初始化pci子系统
    ret = pci_enable_device(pdev);//打开pci设备，允许访问他的bar之类的资源
    if (ret) {
        dev_err(&pdev->dev, "Failed to enable PCI device\n");
        goto err_free_dev;
    }
    ret = pci_request_regions(pdev, DRIVER_NAME);//申请占用设备的bar资源，避免多个驱动同时抢占I/O资源
    if (ret) {
        dev_err(&pdev->dev, "Failed to request regions\n");
        goto err_disable;
    }

    //映射bar0，拿到MMIO寄存器的基址
    dev->bar0_len = pci_resource_len(pdev, 0);
    dev->regs = pci_iomap(pdev, 0, 0);//映射bar0的全部空间
    if (!dev->regs) {
        dev_err(&pdev->dev, "Failed to map BAR0\n");
        ret = -ENOMEM;
        goto err_release;
    }
    //初始化dma
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
        if (ret) {
            dev_err(&pdev->dev, "Failed to set DMA mask\n");
            goto err_unmap;
        }
    }
    //初始化缓存池
    for (i = 0; i < NUM_CHANNELS; i++) {
        dev->channel[i].buffer = kmalloc(CHANNEL_SIZE, GFP_KERNEL);
        if (!dev->channel[i].buffer) {
            dev_err(&pdev->dev, "Failed to alloc buffer for channel %d\n", i);
            ret = -ENOMEM;
            goto err_free_buffers;
        }
    }
    //开启设备的bus master的能力，让PCI设备可以主动发起总线事务
    pci_set_master(pdev);

    //申请中断向量  也就是说申请多个中断类型 最少1个 最多1个
    ret = pci_alloc_irq_vectors(pdev, 1, 1,
                                PCI_IRQ_MSI | PCI_IRQ_MSIX | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "Failed to alloc IRQ vectors\n");
        goto err_free_buffers;
    }
    //申请中断号向量
    dev->irq = pci_irq_vector(pdev, 0);
    //建立中断的回调函数绑定 如果设备是支持msi的话 就启用msi 不是的话 就回退到INTx
    ret = request_threaded_irq(dev->irq, acq_hard_irq, acq_thread_irq,
                               ((pdev->msi_enabled || pdev->msix_enabled) ?
                                0 : IRQF_SHARED),
                               DRIVER_NAME, dev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d\n", dev->irq);
        goto err_free_irq_vectors;
    }

    if (pdev->msix_enabled)
        dev_info(&pdev->dev, "Using MSI-X\n");
    else if (pdev->msi_enabled)
        dev_info(&pdev->dev, "Using MSI\n");
    else
        dev_info(&pdev->dev, "Using legacy INTx\n");
    
    ret = acq_chrdev_init(dev);
    if (ret)
        goto err_free_irq;

    dev_info(&pdev->dev, "Baseline driver loaded (IRQ: %d)\n", dev->irq);
    return 0;

err_free_irq:
    free_irq(dev->irq, dev);
err_free_irq_vectors:
    pci_free_irq_vectors(pdev);
err_free_buffers:
    for (i = 0; i < NUM_CHANNELS; i++)
        kfree(dev->channel[i].buffer);
err_unmap:
    pci_iounmap(pdev, dev->regs);
err_release:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free_dev:
    kfree(dev);
    g_acq_dev = NULL;
    return ret;
}

static void acq_remove(struct pci_dev *pdev)
{
    struct acq_device *dev = pci_get_drvdata(pdev);
    int i;

    dev_info(&pdev->dev, "Removing driver, IRQ: %lu, Overflow: %lu, Transfers: %lu\n",
             dev->irq_count, dev->overflow_count, dev->transfer_count);

    iowrite32(0, dev->regs + REG_CTRL);
    iowrite32(0, dev->regs + REG_IRQ_MASK);

    acq_chrdev_cleanup(dev);

    free_irq(dev->irq, dev);
    pci_free_irq_vectors(pdev);

    for (i = 0; i < NUM_CHANNELS; i++)
        kfree(dev->channel[i].buffer);

    pci_iounmap(pdev, dev->regs);
    pci_release_regions(pdev);
    pci_disable_device(pdev);

    kfree(dev);
    g_acq_dev = NULL;

    dev_info(&pdev->dev, "Driver removed\n");
}

//以下为注册pci子系统的步骤
//1.挂到pci子系统的第一步，设置匹配表 
static const struct pci_device_id acq_pci_ids[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
    { PCI_DEVICE(VENDOR_ID, QEMU_EDU_DEVICE_ID) },
    { 0, }
};
//2.把id表导出到内核模块的符号表，pci子系统枚举的时候，会遍历所有驱动的id表，找到和硬件匹配的驱动
MODULE_DEVICE_TABLE(pci, acq_pci_ids);
//3.定义pci驱动的结构体 绑定id表和回调函数(probe\remove)
static struct pci_driver acq_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = acq_pci_ids,
    .probe = acq_probe,
    .remove = acq_remove,
};
//4.注册pci驱动到pci子系统中
module_pci_driver(acq_pci_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Developer");
MODULE_DESCRIPTION("PCIe Acquisition Card Driver - Baseline Version");
MODULE_VERSION("1.0-baseline");
