all : test.elf test.bin

test.elf : test.c
	riscv64-unknown-elf-gcc -o $@ $^ -O1 -march=rv32ima -mabi=ilp32 -nostdlib -T flatfile.lds

test.bin : test.elf
	riscv64-unknown-elf-objcopy test.elf -O binary test.bin

clean :
	rm -rf test.elf test.bin

