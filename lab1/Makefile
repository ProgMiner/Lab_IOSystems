obj-m += lab1.o

all:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
load:
	sudo insmod lab1.ko
unload:
	sudo rmmod lab1
watch:
	sudo dmesg