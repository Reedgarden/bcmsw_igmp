# kernel directory 
CROSS_COMPILE := mipsel-linux-
ARCH := mips
KDIR := /home/jhkim/workspace/olleh/stblinux-2.6.37
PWD  := $(shell pwd) 

obj-m += bcmsw_proxy.o
bcmsw_proxy-objs := bcmsw_mii.o bcmsw_igmp.o bcmsw_snoop.o bcmsw_proc.o bcmsw_qos.o bcmsw_kwatch.o module_main.o

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

all: module
module:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) modules 
	
clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf *.symvers
	rm -rf *.order
	rm -rf .tmp_versions