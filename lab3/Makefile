obj-m += main.o
all :
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean :
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

load:
	insmod main.ko

unload:
	rmmod main.ko

watch:
	dmesg
