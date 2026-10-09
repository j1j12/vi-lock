KDIR          ?= /home/alientek/linux
ARCH          ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

obj-m += rsc_status_probe.o

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
