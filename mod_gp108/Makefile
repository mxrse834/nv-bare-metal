obj-m += temp_P2.o

kdir := /lib/modules/$(shell uname -r)/build
pwd := $(shell pwd)

all : 
	make -C $(kdir) M=$(pwd) modules
clean:
	make -C /lib/modules/$(kdir)/build M=$(pwd) clean
