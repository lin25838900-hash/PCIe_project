obj-m += pcie_acq_baseline.o
pcie_acq_baseline-objs := acq_main.o acq_data.o acq_irq.o acq_dma.o acq_chrdev.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ccflags-y += -Wall -g

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
