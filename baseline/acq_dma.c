#include <linux/barrier.h>
#include <linux/jiffies.h>

#include "pcie_acq_baseline.h"

#ifndef DMA_ST_DONE
#define DMA_ST_DONE   BIT(0)
#endif
#ifndef DMA_ST_ERR
#define DMA_ST_ERR    BIT(1)
#endif

int acq_dma_setup_sg(struct acq_device *dev)
{
    int i;

    sg_init_table(dev->sg, NUM_CHANNELS);
    dev->sg_desc = NULL;
    dev->sg_desc_dma = 0;
    dev->sg_desc_count = 0;

    for (i = 0; i < NUM_CHANNELS; i++) {
        if (!dev->channel[i].buffer)
            return -EINVAL;
        sg_set_buf(&dev->sg[i], dev->channel[i].buffer, CHANNEL_SIZE);
        sg_dma_address(&dev->sg[i]) = dev->channel[i].dma_addr;
        sg_dma_len(&dev->sg[i]) = CHANNEL_SIZE;
    }

    dev->sg_desc_count = NUM_CHANNELS;
    dev->sg_desc = dma_alloc_coherent(&dev->pdev->dev,
                                      sizeof(*dev->sg_desc) *
                                      dev->sg_desc_count,
                                      &dev->sg_desc_dma,
                                      GFP_KERNEL);
    if (!dev->sg_desc) {
        dev->sg_desc_count = 0;
        dev->sg_desc_dma = 0;
        return -ENOMEM;
    }

    for (i = 0; i < dev->sg_desc_count; i++) {
        dev->sg_desc[i].addr = sg_dma_address(&dev->sg[i]);
        dev->sg_desc[i].len = sg_dma_len(&dev->sg[i]);
        dev->sg_desc[i].rsvd = 0;
    }

    return 0;
}

void acq_dma_cleanup_sg(struct acq_device *dev)
{
    if (!dev->sg_desc)
        return;

    dma_free_coherent(&dev->pdev->dev,
                      sizeof(*dev->sg_desc) * dev->sg_desc_count,
                      dev->sg_desc, dev->sg_desc_dma);
    dev->sg_desc = NULL;
    dev->sg_desc_dma = 0;
    dev->sg_desc_count = 0;
}

static int acq_dma_transfer_sg(struct acq_device *dev)
{
    dma_addr_t sg_base;
    long tmo;
    u32 status;
    unsigned long flags;

    if (!dev->sg_desc || dev->sg_desc_count == 0)
        return -EINVAL;

    sg_base = dev->sg_desc_dma;

    reinit_completion(&dev->dma_done);
    spin_lock_irqsave(&dev->dma_lock, flags);
    dev->dma_busy = true;
    dev->last_dma_status = 0;
    spin_unlock_irqrestore(&dev->dma_lock, flags);

    iowrite32(lower_32_bits(sg_base), dev->regs + REG_DMA_SG_ADDR_LO);
    iowrite32(upper_32_bits(sg_base), dev->regs + REG_DMA_SG_ADDR_HI);
    iowrite32(dev->sg_desc_count, dev->regs + REG_DMA_SG_COUNT);
    wmb();
    iowrite32(DMA_CTRL_START | DMA_CTRL_SG_MODE, dev->regs + REG_DMA_CTRL);

    tmo = wait_for_completion_timeout(&dev->dma_done,
                                      msecs_to_jiffies(1000));

    if (tmo == 0) {
        spin_lock_irqsave(&dev->dma_lock, flags);
        dev->dma_busy = false;
        spin_unlock_irqrestore(&dev->dma_lock, flags);
        dev_err(&dev->pdev->dev, "SG DMA timeout\n");
        return -ETIMEDOUT;
    }

    status = dev->last_dma_status;
    if (status & DMA_ST_ERR) {
        dev_err(&dev->pdev->dev, "SG DMA error, status=0x%x\n", status);
        return -EIO;
    }

    return 0;
}


int acq_dma_transfer_all(struct acq_device *dev)
{
    if (!dev->sg_desc_count)
        return -EINVAL;

    return acq_dma_transfer_sg(dev);
}
