#include "pcie_acq_optimized.h"

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
    return IRQ_WAKE_THREAD;
}

irqreturn_t acq_thread_irq(int irq, void *dev_id)
{
    struct acq_device *dev = dev_id;

    acq_copy_from_mmio(dev);
    acq_process_buffers(dev);

    return IRQ_HANDLED;
}
