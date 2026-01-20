#ifndef PCIE_ACQ_BASELINE_H
#define PCIE_ACQ_BASELINE_H

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/scatterlist.h>

/* 模块/设备命名 */
#define DRIVER_NAME     "pcie_acq"
#define DEVICE_NAME     "pcie_acq"
#define CLASS_NAME      "pcie_acq_class"

/* PCI 设备 ID */
#define VENDOR_ID       0x1234
#define DEVICE_ID       0x5678

/* 采集通道与布局 */
#define NUM_CHANNELS    8
#define CHANNEL_SIZE    4096
#define CHANNEL_OFFSET  0x1000

/* 寄存器偏移 */
#define REG_CTRL        0x0010
#define REG_STATUS      0x0014
#define REG_IRQ_STATUS  0x0020
#define REG_IRQ_MASK    0x0024
#define REG_IRQ_CLEAR   0x0028
#define REG_DMA_CTRL    0x0100
#define REG_DMA_STATUS  0x0104
#define REG_DMA_ADDR_LO 0x0108
#define REG_DMA_ADDR_HI 0x010C
#define REG_DMA_LENGTH  0x0110
#define REG_DMA_SG_ADDR_LO 0x0114
#define REG_DMA_SG_ADDR_HI 0x0118
#define REG_DMA_SG_COUNT 0x011C
#define REG_CH_BASE     0x1000

/* 控制/中断位 */
#define CTRL_ENABLE     BIT(0)
#define IRQ_DATA_READY  BIT(0)
#define IRQ_DMA_DONE    BIT(1)
#define IRQ_OVERFLOW    BIT(2)
#define DMA_CTRL_START  BIT(0)
#define DMA_CTRL_SG_MODE BIT(1)

struct acq_sg_desc {
    u64 addr;
    u32 len;
    u32 rsvd;
};

/* 每通道数据结构 */
struct acq_channel {
    void *buffer;
    dma_addr_t dma_addr;
    u32 checksum;
    bool data_ready;
};

/* 设备私有数据 */
struct acq_device {
    struct pci_dev *pdev;
    void __iomem *regs;
    resource_size_t bar0_len;
    int irq;

    struct acq_channel channel[NUM_CHANNELS];
    struct scatterlist sg[NUM_CHANNELS];
    struct acq_sg_desc *sg_desc;
    dma_addr_t sg_desc_dma;
    u32 sg_desc_count;

    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t devno;

    /* read() 等待数据就绪 */
    wait_queue_head_t wait_queue;
    bool data_available;

    /* DMA 完成等待 */
    struct completion dma_done;
    spinlock_t dma_lock;
    bool dma_busy;
    u32 last_dma_status;

    /* 统计计数 */
    unsigned long irq_count;
    unsigned long overflow_count;
    unsigned long transfer_count;
    u32 last_irq_status;

};

extern struct acq_device *g_acq_dev;

/* 数据处理 */
u32 acq_calc_checksum(void *data, size_t len);
void acq_copy_from_mmio(struct acq_device *dev);
void acq_process_buffers(struct acq_device *dev);

/* 中断处理 */
irqreturn_t acq_hard_irq(int irq, void *dev_id);
irqreturn_t acq_thread_irq(int irq, void *dev_id);

/* DMA 传输（SG 批量模式） */
int acq_dma_transfer_all(struct acq_device *dev);
int acq_dma_setup_sg(struct acq_device *dev);
void acq_dma_cleanup_sg(struct acq_device *dev);

/* 字符设备接口 */
int acq_chrdev_init(struct acq_device *dev);
void acq_chrdev_cleanup(struct acq_device *dev);

#endif
