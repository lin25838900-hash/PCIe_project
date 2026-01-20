#include <linux/barrier.h>
#include <linux/jiffies.h>

#include "pcie_acq_baseline.h"

#ifndef DMA_ST_DONE
#define DMA_ST_DONE   BIT(0)
#endif
#ifndef DMA_ST_ERR
#define DMA_ST_ERR    BIT(1)
#endif

int acq_dma_transfer_channel(struct acq_device *dev, int ch)
{
    dma_addr_t dma_addr; // 存放DMA可访问的物理地址
    long tmo;            // 等待中断的超时（jiffies）
    u32 status;          // 存放DMA状态寄存器值
    unsigned long flags;

    // 步骤1：DMA内存映射（最核心！）
    dma_addr = dma_map_single(&dev->pdev->dev,  // PCI设备结构体
                              dev->channel[ch].buffer,  // 内核缓冲区（虚拟地址）
                              CHANNEL_SIZE,  // 传输长度
                              DMA_FROM_DEVICE); // 传输方向：外设→内存
    // 检查映射是否失败
    if (dma_mapping_error(&dev->pdev->dev, dma_addr)) {
        dev_err(&dev->pdev->dev, "DMA mapping failed for channel %d\n", ch);
        return -ENOMEM;
    }
    //重新初始化完成量是，适用于多次DMA传输（复用同一个完成量）
    reinit_completion(&dev->dma_done);
    spin_lock_irqsave(&dev->dma_lock, flags);
    dev->dma_busy = true;
    dev->last_dma_status = 0;
    spin_unlock_irqrestore(&dev->dma_lock, flags);

    // 步骤2：配置采集卡的DMA寄存器（告诉硬件传输参数）
    // 写DMA地址低32位到寄存器
    iowrite32(lower_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_LO);
    // 写DMA地址高32位到寄存器（支持64位系统）
    iowrite32(upper_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_HI);
    // 写DMA传输长度到寄存器
    iowrite32(CHANNEL_SIZE, dev->regs + REG_DMA_LENGTH);
    // 写控制寄存器：启动DMA传输（0x01是启动指令，硬件相关）
    wmb();
    iowrite32(0x01, dev->regs + REG_DMA_CTRL);

    // 步骤3：等待DMA完成中断（睡眠等待）
    tmo = wait_for_completion_timeout(&dev->dma_done,
                                      msecs_to_jiffies(1000));

    // 步骤4：解除DMA映射（无论成功/失败，都要解映射！）
    dma_unmap_single(&dev->pdev->dev, dma_addr, CHANNEL_SIZE, DMA_FROM_DEVICE);

    // 步骤5：检查是否超时
    if (tmo == 0) {
        spin_lock_irqsave(&dev->dma_lock, flags);
        dev->dma_busy = false;
        spin_unlock_irqrestore(&dev->dma_lock, flags);
        dev_err(&dev->pdev->dev, "DMA timeout for channel %d\n", ch);
        return -ETIMEDOUT;
    }

    status = dev->last_dma_status;
    if (status & DMA_ST_ERR) {
        dev_err(&dev->pdev->dev, "DMA error on channel %d, status=0x%x\n",
                ch, status);
        return -EIO;
    }

    // 传输成功
    return 0;
}

int acq_dma_transfer_all(struct acq_device *dev)
{
    int i, ret;

    for (i = 0; i < NUM_CHANNELS; i++) {
        ret = acq_dma_transfer_channel(dev, i);
        if (ret)
            return ret;
    }

    return 0;
}
