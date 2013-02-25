VERSION = 2
PATCHLEVEL = 6
SUBLEVEL = 29
EXTRAVERSION = -00054-g5f01537

obj-m += debug_device.o

# Cambiar al directorio del fuente del kernel
KDIR = __PATH TO THE KERNEL SOURCE__
# Cambiar al directorio de las herramientas para hacer cross compiling
CROSS_COMPILE= __PATH TO YOUR CROSS COMPILING TOOLS__
PWD := $(shell pwd)

all:
	make -C $(KDIR) ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} EXTRA_CFLAGS=-fno-pic -Wall -Wextra -Wwrite-strings   SUBDIRS=$(PWD) modules
	rm -rf *.c~
	rm -rf *.mod*
	rm -rf *.o

clean:
	make -C $(KDIR) ARCH=arm CROSS_COMPILE={CROSS_COMPILE} EXTRA_CFLAGS=-fno-pic SUBDIRS=$(PWD) clean
