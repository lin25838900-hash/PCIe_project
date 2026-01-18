#include "pcie_acq_optimized.h"

u32 acq_calc_checksum(void *data, size_t len)
{
    u32 *ptr = data;
    u32 sum = 0;
    size_t i;

    for (i = 0; i < len / sizeof(u32); i++)
        sum += ptr[i];

    return sum;
}

void acq_copy_from_mmio(struct acq_device *dev)
{
    int ch;

    for (ch = 0; ch < NUM_CHANNELS; ch++) {
        size_t offset = REG_CH_BASE + ch * CHANNEL_OFFSET;

        if (offset + CHANNEL_SIZE <= dev->bar0_len) {
            memcpy(dev->channel[ch].buffer,
                   dev->regs + offset,
                   CHANNEL_SIZE);
        } else {
            memset(dev->channel[ch].buffer, 0, CHANNEL_SIZE);
        }
    }
}

void acq_process_buffers(struct acq_device *dev)
{
    int i;

    for (i = 0; i < NUM_CHANNELS; i++) {
        dev->channel[i].checksum = acq_calc_checksum(dev->channel[i].buffer,
                                                     CHANNEL_SIZE);
        dev->channel[i].data_ready = true;
    }

    spin_lock(&dev->lock);
    dev->data_available = true;
    spin_unlock(&dev->lock);
    wake_up_interruptible(&dev->wait_queue);
}
