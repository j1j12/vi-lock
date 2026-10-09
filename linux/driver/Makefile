# Out-of-tree kernel module Makefile for access_control
#
# Build in the VM (Linux):
#   export PATH=/usr/local/arm/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin:$PATH
#   make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- KDIR=/home/alientek/linux
#
# or override KDIR/CROSS_COMPILE via command line.

KDIR          ?= /home/alientek/linux
ARCH          ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

obj-m += access_control.o

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
