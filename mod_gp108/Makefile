obj-m += temp_P3.o

kdir := /lib/modules/$(shell uname -r)/build
src := $(shell pwd)
out := $(src)/build

.PHONY: all clean

all : 
	$(MAKE) -C $(kdir) M=$(src) MO=$(out) modules
clean:
	$(MAKE) -C $(kdir) M=$(src) MO=$(out) clean
