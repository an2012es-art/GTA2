# تجميع المشروع: اكتب  make  داخل هذا المجلد (بعد تثبيت devkitPro + libnds)

ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set. Install devkitPro and set DEVKITARM)
endif

DEVKITPRO ?= /opt/devkitpro
LIBNDS    := $(DEVKITPRO)/libnds

export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

ARCH    := -march=armv5te -mtune=arm946e-s -mthumb -mthumb-interwork
CFLAGS  := -O2 -Wall -ffast-math -fomit-frame-pointer $(ARCH) -I$(LIBNDS)/include -DARM9
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -L$(LIBNDS)/lib -lnds9

all: citydrive.nds

citydrive.elf: main.c
	arm-none-eabi-gcc $(CFLAGS) main.c $(LDFLAGS) -o $@

citydrive.nds: citydrive.elf
	ndstool -c $@ -9 citydrive.elf

clean:
	rm -f citydrive.elf citydrive.nds
