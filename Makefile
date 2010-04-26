all: sersniff.hex

CC     = ba-elf-gcc
AR     = ba-elf-ar
OBJC   = ba-elf-objcopy

CFLAGS += -Wall -fomit-frame-pointer -O0 -g
CFLAGS += -mno-entri -mno-multi -mno-setcc -mno-cmov -msibcall
CFLAGS += -mno-carry -mno-subb -mno-sext -mno-ror
CFLAGS += -mno-ff1 -mno-hard-div -mhard-mul -mbranch-cost=3
CFLAGS += -msimple-mul -mabi=1 -march=ba1 -mredzone-size=4
CFLAGS += -DOR1K -DPCB_DEVKIT2 -DEMBEDDED -DLEAN_N_MEAN -DSINGLE_CONTEXT -DCHIP_RELEASE_5131

#CFLAGS += -Ijennic-sdk/Platform/DK2/Include
#CFLAGS += -Ijennic-sdk/Chip/JN513xR1/Include
#CFLAGS += -Ijennic-sdk/Common/Include
#CFLAGS += -Ijennic-sdk/Platform/Common/Include
#CFLAGS += -Ijennic-sdk/Chip/Common/Include

LDFLAGS = -Wl,-Map=map -TAppBuild_JN5139R1.ld -nostdlib
LDLIBS = -lChipLib -lc

sersniff: sersniff.c sprintf.c
sersniff.hex: sersniff
	$(OBJC) -S -O binary $< $@

clean:
	rm -fr *.o *~ *.hex sersniff
