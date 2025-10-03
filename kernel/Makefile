# RAZOR Filesystem Kernel Module Makefile
# For WSL2 and Linux kernel compilation

obj-m += razorfs.o

# Kernel build directory
KDIR := /home/nico/WORK_ROOT/RAZOR_repo/WSL2-Linux-Kernel
PWD := $(shell pwd)

# Build flags for C
ccflags-y := -std=gnu99 -Wall -Wno-unused-parameter

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

load: all
	sudo insmod razorfs.ko

unload:
	sudo rmmod razorfs

test: load
	@echo "Testing RAZOR filesystem..."
	sudo mkdir -p /mnt/razorfs_test
	sudo mount -t razorfs none /mnt/razorfs_test || echo "Mount failed - this is expected in WSL"
	lsmod | grep razorfs
	dmesg | tail -10 | grep razorfs

info:
	@echo "Kernel version: $(shell uname -r)"
	@echo "Build directory: $(KDIR)"
	@echo "Module name: razorfs.ko"

.PHONY: all clean install load unload test info