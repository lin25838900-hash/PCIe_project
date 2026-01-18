#include "pcie_acq_baseline.h"

//中断处理函数 当触发中断的时候 会抢占性回调这个函数

irqreturn_t acq_irq_handler(int irq, void *dev_id)
{
    struct acq_device *dev = dev_id;
    u32 status;

    status = ioread32(dev->regs + REG_IRQ_STATUS);
    //判断是否为本设备的中断，不是的话 直接返回无中断
    if (!(status & (IRQ_DATA_READY | IRQ_OVERFLOW)))
        return IRQ_NONE;

    //此时可以走到这里了 说明status全是0（32位），然后把中断寄存器置为0，清理中断
    iowrite32(status, dev->regs + REG_IRQ_CLEAR);
    //检查溢出的问题  当数据溢出之后 pcie采集卡（硬件） 会把这个位置置为1  意思是有数据溢出 需要处理 
    if (status & IRQ_OVERFLOW)
        dev->overflow_count++;
        
    //如果是数据准备完毕的话 发生中断  就要开始移动数据了
    if (status & IRQ_DATA_READY) {
        acq_copy_from_mmio(dev);
        acq_process_buffers(dev);
    }

    dev->irq_count++;
    if (status & IRQ_DATA_READY) {
        dev_info_ratelimited(&dev->pdev->dev,
                             "IRQ status=0x%08x irq=%lu transfers=%lu\n",
                             status, dev->irq_count, dev->transfer_count);
    }
    return IRQ_HANDLED;
}
