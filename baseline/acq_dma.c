#include "pcie_acq_baseline.h"

int acq_dma_transfer_channel(struct acq_device *dev, int ch)
{
    dma_addr_t dma_addr; // 存放DMA可访问的物理地址
    int timeout = 1000;  // 超时计数（防止无限等待）
    u32 status;          // 存放DMA状态寄存器值

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

    // 步骤2：配置采集卡的DMA寄存器（告诉硬件传输参数）
    // 写DMA地址低32位到寄存器
    iowrite32(lower_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_LO);
    // 写DMA地址高32位到寄存器（支持64位系统）
    iowrite32(upper_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_HI);
    // 写DMA传输长度到寄存器
    iowrite32(CHANNEL_SIZE, dev->regs + REG_DMA_LENGTH);
    // 写控制寄存器：启动DMA传输（0x01是启动指令，硬件相关）
    iowrite32(0x01, dev->regs + REG_DMA_CTRL);

    // 步骤3：轮询等待DMA传输完成（忙等+超时保护）
    while (timeout--) {
        // 读DMA状态寄存器
        status = ioread32(dev->regs + REG_DMA_STATUS);
        // 如果状态寄存器的0位为1（传输完成），跳出循环
        if (status & 0x01)
            break;
        // 等待10微秒，再检查（避免CPU空转太狠）
        udelay(10);
    }

    // 步骤4：解除DMA映射（无论成功/失败，都要解映射！）
    dma_unmap_single(&dev->pdev->dev, dma_addr, CHANNEL_SIZE, DMA_FROM_DEVICE);

    // 步骤5：检查是否超时
    if (timeout <= 0) {
        dev_err(&dev->pdev->dev, "DMA timeout for channel %d\n", ch);
        return -ETIMEDOUT;
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
