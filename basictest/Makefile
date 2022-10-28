TARGETS:=test.elf test.bin test.debug.txt

all : $(TARGETS)


CFLAGS:=-g -O1 -march=rv32ima -mabi=ilp32
LDFLAGS:= -T flatfile.lds -nostdlib

test.elf : test.c
	riscv64-unknown-elf-gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

test.debug.txt : test.elf
	riscv64-unknown-elf-objdump -t $^ > $@
	riscv64-unknown-elf-objdump -S $^ >> $@

test.bin : test.elf
	riscv64-unknown-elf-objcopy test.elf -O binary test.bin

postlist : 
	

clean :
	rm -rf $(TARGETS)  test.debug.txt

