#include "pcie_acq_optimized.h"

int acq_setup_dma(struct acq_device *dev)
{
    int i;

    sg_init_table(dev->sg, NUM_CHANNELS);

    for (i = 0; i < NUM_CHANNELS; i++) {
        dev->channel[i].buffer = dma_alloc_coherent(&dev->pdev->dev,
                                                    CHANNEL_SIZE,
                                                    &dev->channel[i].dma_addr,
                                                    GFP_KERNEL);
        if (!dev->channel[i].buffer) {
            dev_err(&dev->pdev->dev,
                    "Failed to alloc DMA buffer for channel %d\n", i);
            goto err_free;
        }

        sg_set_buf(&dev->sg[i], dev->channel[i].buffer, CHANNEL_SIZE);
        sg_dma_address(&dev->sg[i]) = dev->channel[i].dma_addr;
        sg_dma_len(&dev->sg[i]) = CHANNEL_SIZE;
    }

    dev->sg_mapped = dma_map_sg(&dev->pdev->dev, dev->sg,
                               NUM_CHANNELS, DMA_FROM_DEVICE);
    if (dev->sg_mapped == 0) {
        dev_err(&dev->pdev->dev, "Failed to map SG list\n");
        goto err_free;
    }

    dev_info(&dev->pdev->dev, "DMA setup complete, %d SG entries\n",
             dev->sg_mapped);
    return 0;

err_free:
    for (i = 0; i < NUM_CHANNELS; i++) {
        if (dev->channel[i].buffer) {
            dma_free_coherent(&dev->pdev->dev, CHANNEL_SIZE,
                              dev->channel[i].buffer,
                              dev->channel[i].dma_addr);
        }
    }
    return -ENOMEM;
}

void acq_cleanup_dma(struct acq_device *dev)
{
    int i;

    if (dev->sg_mapped)
        dma_unmap_sg(&dev->pdev->dev, dev->sg, NUM_CHANNELS, DMA_FROM_DEVICE);

    for (i = 0; i < NUM_CHANNELS; i++) {
        if (dev->channel[i].buffer) {
            dma_free_coherent(&dev->pdev->dev, CHANNEL_SIZE,
                              dev->channel[i].buffer,
                              dev->channel[i].dma_addr);
        }
    }
}
