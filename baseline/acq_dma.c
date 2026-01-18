#include "pcie_acq_baseline.h"

int acq_dma_transfer_channel(struct acq_device *dev, int ch)
{
    dma_addr_t dma_addr;
    int timeout = 1000;
    u32 status;

    dma_addr = dma_map_single(&dev->pdev->dev,
                              dev->channel[ch].buffer,
                              CHANNEL_SIZE,
                              DMA_FROM_DEVICE);
    if (dma_mapping_error(&dev->pdev->dev, dma_addr)) {
        dev_err(&dev->pdev->dev, "DMA mapping failed for channel %d\n", ch);
        return -ENOMEM;
    }

    iowrite32(lower_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_LO);
    iowrite32(upper_32_bits(dma_addr), dev->regs + REG_DMA_ADDR_HI);
    iowrite32(CHANNEL_SIZE, dev->regs + REG_DMA_LENGTH);
    iowrite32(0x01, dev->regs + REG_DMA_CTRL);

    while (timeout--) {
        status = ioread32(dev->regs + REG_DMA_STATUS);
        if (status & 0x01)
            break;
        udelay(10);
    }

    dma_unmap_single(&dev->pdev->dev, dma_addr, CHANNEL_SIZE, DMA_FROM_DEVICE);

    if (timeout <= 0) {
        dev_err(&dev->pdev->dev, "DMA timeout for channel %d\n", ch);
        return -ETIMEDOUT;
    }

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
