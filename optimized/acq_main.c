/*
 * PCIe Acquisition Card Driver - Optimized Version (modularized)
 */

#include "pcie_acq_optimized.h"

struct acq_device *g_acq_dev;

static int acq_suspend(struct device *dev)
{
    struct pci_dev *pdev = to_pci_dev(dev);
    struct acq_device *acq = pci_get_drvdata(pdev);

    iowrite32(0, acq->regs + REG_CTRL);

    pci_save_state(pdev);
    pci_set_power_state(pdev, PCI_D3hot);

    dev_info(dev, "Suspended, entering D3hot\n");
    return 0;
}

static int acq_resume(struct device *dev)
{
    struct pci_dev *pdev = to_pci_dev(dev);

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);

    dev_info(dev, "Resumed to D0\n");
    return 0;
}

static int acq_runtime_suspend(struct device *dev)
{
    struct pci_dev *pdev = to_pci_dev(dev);

    pci_save_state(pdev);
    pci_set_power_state(pdev, PCI_D3hot);
    return 0;
}

static int acq_runtime_resume(struct device *dev)
{
    struct pci_dev *pdev = to_pci_dev(dev);

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);
    return 0;
}

static const struct dev_pm_ops acq_pm_ops = {
    .suspend = acq_suspend,
    .resume = acq_resume,
    .runtime_suspend = acq_runtime_suspend,
    .runtime_resume = acq_runtime_resume,
};

static int acq_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct acq_device *dev;
    int ret;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->pdev = pdev;
    pci_set_drvdata(pdev, dev);
    g_acq_dev = dev;

    init_waitqueue_head(&dev->wait_queue);
    spin_lock_init(&dev->lock);

    ret = pcim_enable_device(pdev);
    if (ret)
        goto err_clear;

    ret = pcim_iomap_regions(pdev, BIT(0), DRIVER_NAME);
    if (ret)
        goto err_clear;

    dev->regs = pcim_iomap_table(pdev)[0];
    dev->bar0_len = pci_resource_len(pdev, 0);

    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret)
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (ret)
        goto err_clear;

    ret = acq_setup_dma(dev);
    if (ret)
        goto err_clear;

    pci_set_master(pdev);

    dev->irq = pdev->irq;
    ret = devm_request_threaded_irq(&pdev->dev, dev->irq,
                                    acq_hard_irq, acq_thread_irq,
                                    IRQF_SHARED | IRQF_ONESHOT,
                                    DRIVER_NAME, dev);
    if (ret)
        goto err_dma;

    ret = acq_chrdev_init(dev);
    if (ret)
        goto err_dma;

    dev_info(&pdev->dev, "Optimized driver loaded (IRQ: %d)\n", dev->irq);
    return 0;

err_dma:
    acq_cleanup_dma(dev);
err_clear:
    g_acq_dev = NULL;
    return ret;
}

static void acq_remove(struct pci_dev *pdev)
{
    struct acq_device *dev = pci_get_drvdata(pdev);

    iowrite32(0, dev->regs + REG_CTRL);

    acq_chrdev_cleanup(dev);
    acq_cleanup_dma(dev);

    g_acq_dev = NULL;

    dev_info(&pdev->dev, "Removed, IRQ: %lu, Overflow: %lu\n",
             dev->irq_count, dev->overflow_count);
}

static const struct pci_device_id acq_ids[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
    { PCI_DEVICE(VENDOR_ID, QEMU_EDU_DEVICE_ID) },
    { 0 }
};
MODULE_DEVICE_TABLE(pci, acq_ids);

static struct pci_driver acq_driver = {
    .name = DRIVER_NAME,
    .id_table = acq_ids,
    .probe = acq_probe,
    .remove = acq_remove,
    .driver = {
        .pm = &acq_pm_ops,
    },
};

module_pci_driver(acq_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PCIe Acquisition Driver - Optimized Version");
MODULE_VERSION("2.0");
