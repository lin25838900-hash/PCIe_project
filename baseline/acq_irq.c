#include "pcie_acq_baseline.h"

irqreturn_t acq_hard_irq(int irq, void *dev_id)
{
    struct acq_device *dev = dev_id;
    u32 status;

    status = ioread32(dev->regs + REG_IRQ_STATUS);

    if (!(status & (IRQ_DATA_READY | IRQ_OVERFLOW)))
        return IRQ_NONE;

    iowrite32(status, dev->regs + REG_IRQ_CLEAR);

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

    acq_copy_from_mmio(dev);
    acq_process_buffers(dev);
    dev_info_ratelimited(&dev->pdev->dev,
                         "IRQ status=0x%08x irq=%lu transfers=%lu\n",
                         dev->last_irq_status, dev->irq_count,
                         dev->transfer_count);
    return IRQ_HANDLED;
}
