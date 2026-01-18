#ifndef PCIE_ACQ_OPTIMIZED_H
#define PCIE_ACQ_OPTIMIZED_H

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/err.h>

#define DRIVER_NAME     "pcie_acq"
#define DEVICE_NAME     "pcie_acq"
#define CLASS_NAME      "pcie_acq_class"

#define VENDOR_ID       0x1234
#define DEVICE_ID       0x5678
#define QEMU_EDU_DEVICE_ID 0x11e8

#define NUM_CHANNELS    8
#define CHANNEL_SIZE    4096
#define CHANNEL_OFFSET  0x1000

#define REG_CTRL        0x0010
#define REG_IRQ_STATUS  0x0020
#define REG_IRQ_CLEAR   0x0028
#define REG_CH_BASE     0x1000

#define CTRL_ENABLE     BIT(0)
#define IRQ_DATA_READY  BIT(0)
#define IRQ_OVERFLOW    BIT(2)

struct acq_channel {
    void *buffer;
    dma_addr_t dma_addr;
    u32 checksum;
    bool data_ready;
};

struct acq_device {
    struct pci_dev *pdev;
    void __iomem *regs;
    resource_size_t bar0_len;
    int irq;

    struct acq_channel channel[NUM_CHANNELS];
    struct scatterlist sg[NUM_CHANNELS];
    int sg_mapped;

    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t devno;

    wait_queue_head_t wait_queue;
    bool data_available;
    spinlock_t lock;

    unsigned long irq_count;
    unsigned long overflow_count;

};

extern struct acq_device *g_acq_dev;

u32 acq_calc_checksum(void *data, size_t len);
void acq_copy_from_mmio(struct acq_device *dev);
void acq_process_buffers(struct acq_device *dev);

irqreturn_t acq_hard_irq(int irq, void *dev_id);
irqreturn_t acq_thread_irq(int irq, void *dev_id);

int acq_setup_dma(struct acq_device *dev);
void acq_cleanup_dma(struct acq_device *dev);

int acq_chrdev_init(struct acq_device *dev);
void acq_chrdev_cleanup(struct acq_device *dev);

#endif
