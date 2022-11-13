TARGETS:=test.elf test.bin test.debug.txt

all : $(TARGETS)

#PREFIX:=riscv64-unknown-elf-
#CFLAGS:=-I/usr/include

PREFIX:=../buildroot-2022.02.6/output/host/bin/riscv32-buildroot-linux-gnu-
CFLAGS:=-fno-stack-protector

CFLAGS+=-g -O1 -march=rv32imaf -mabi=ilp32f 
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

