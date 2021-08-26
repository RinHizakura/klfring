MODULENAME := klfring
obj-m += $(MODULENAME).o
$(MODULENAME)-y += module.o lfring.o

GIT_HOOKS := .git/hooks/applied

KERNELDIR ?= /lib/modules/`uname -r`/build
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

check: all 
	sudo rmmod klfring || echo
	sudo insmod klfring.ko
	sudo rmmod klfring

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
