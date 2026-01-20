#include "pcie_acq_baseline.h"

irqreturn_t acq_hard_irq(int irq, void *dev_id)
{
    struct acq_device *dev = dev_id;
    u32 status;
    u32 dma_status = 0;
    unsigned long flags;

    status = ioread32(dev->regs + REG_IRQ_STATUS);

    if (!(status & (IRQ_DATA_READY | IRQ_OVERFLOW | IRQ_DMA_DONE)))
        return IRQ_NONE;

    if (status & IRQ_DMA_DONE)
        dma_status = ioread32(dev->regs + REG_DMA_STATUS);

    iowrite32(status, dev->regs + REG_IRQ_CLEAR);

    if (status & IRQ_DMA_DONE) {
        spin_lock_irqsave(&dev->dma_lock, flags);
        if (dev->dma_busy) {
            dev->last_dma_status = dma_status;
            dev->dma_busy = false;
            complete(&dev->dma_done);
        }
        spin_unlock_irqrestore(&dev->dma_lock, flags);
    }

    if (status & IRQ_OVERFLOW)
        dev->overflow_count++;

    dev->irq_count++;
    dev->last_irq_status = status;

    if (status & IRQ_DATA_READY)
        return IRQ_WAKE_THREAD;

    return IRQ_HANDLED;
}

irqreturn_t acq_thread_irq(int irq, void *dev_id)
{
    struct acq_device *dev = dev_id;
    int ret;

    ret = acq_dma_transfer_all(dev);
    if (ret) {
        dev_err_ratelimited(&dev->pdev->dev,
                            "DMA transfer failed: %d\n", ret);
        return IRQ_HANDLED;
    }

    acq_process_buffers(dev);
    dev_info_ratelimited(&dev->pdev->dev,
                         "IRQ status=0x%08x irq=%lu transfers=%lu\n",
                         dev->last_irq_status, dev->irq_count,
                         dev->transfer_count);
    return IRQ_HANDLED;
}
