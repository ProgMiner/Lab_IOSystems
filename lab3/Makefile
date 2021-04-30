obj-m += lab3.o

lab3-objs := lab3_main.o lab3_history.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	sudo insmod lab3.ko

unload:
	sudo rmmod lab3.ko

watch:
	dmesg
