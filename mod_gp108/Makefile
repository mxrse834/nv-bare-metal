obj-m += temp_c.o

kdir := /lib/modules/$(shell uname -r)/build
pwd := $(shell pwd)

all : 
	make -C $(kdir) M=$(pwd) modules
clean:
	make -C $(kdir) M=$(pwd) clean
