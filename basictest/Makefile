TARGETS:=test.elf test.bin test.debug.txt

all : $(TARGETS)

#PREFIX:=riscv64-unknown-elf-
#CFLAGS:=-I/usr/include

PREFIX:=../buildroot-2022.02.6/output/host/bin/riscv32-buildroot-linux-gnu-
CFLAGS:=-fno-stack-protector
CFLAGS+=-static-libgcc -ffunction-sections -fdata-sections  -Wl,-static -fdata-sections -ffunction-sections -Wl,--gc-sections

CFLAGS+=-g -Os -march=rv32imaf -mabi=ilp32 -static
LDFLAGS:= -T flatfile.lds -nostdlib

test.elf : test.c
	$(PREFIX)gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

test.debug.txt : test.elf
	$(PREFIX)objdump -t $^ > $@
	$(PREFIX)objdump -S $^ >> $@

test.bin : test.elf
	$(PREFIX)objcopy test.elf -O binary test.bin

postlist : 
	

clean :
	rm -rf $(TARGETS)  test.debug.txt

