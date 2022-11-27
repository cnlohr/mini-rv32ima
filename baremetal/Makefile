PROJECT:=baremetal
TARGETS:=$(PROJECT).elf $(PROJECT).bin $(PROJECT).debug.txt

all : $(TARGETS)

#PREFIX:=riscv64-unknown-elf-
#CFLAGS:=-I/usr/include

PREFIX:=../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-
CFLAGS:=-fno-stack-protector
CFLAGS+=-static-libgcc -ffunction-sections -fdata-sections  -Wl,-static -fdata-sections -ffunction-sections -Wl,--gc-sections
CFLAGS+=-g -Os -march=rv32ima -mabi=ilp32 -static
LDFLAGS:= -T flatfile.lds -nostdlib

$(PROJECT).elf : $(PROJECT).c $(PROJECT).S
	$(PREFIX)gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(PROJECT).debug.txt : $(PROJECT).elf
	$(PREFIX)objdump -t $^ > $@
	$(PREFIX)objdump -S $^ >> $@

$(PROJECT).bin : $(PROJECT).elf
	$(PREFIX)objcopy $^ -O binary $@

test : $(PROJECT).bin
	../mini-rv32ima/mini-rv32ima -f $<

clean :
	rm -rf $(TARGETS)

