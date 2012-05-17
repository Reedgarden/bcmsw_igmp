# kernel directory 
KERNELDIR = /home/jhkim/workspace/olleh/stblinux-2.6.37

# module files
MODULES := bcmsw_proxy.o

CROSS_COMPILE := mipsel-linux-
ARCH := mips
KDIR := $(KERNELDIR)
PWD  := $(shell pwd) 

obj-m := $(MODULES)

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

all: module
module:
	$(MAKEARCH) -C $(KDIR) SUBDIRS=$(PWD) modules
	
clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf *.symvers
	rm -rf *.order
	rm -rf .tmp_versions