all : hello_linux.flt

hello_linux.flt : hello_linux.c
	../buildroot/output/host/bin/riscv32-linux-gcc -march=rv32ima -mabi=ilp32 -o hello_linux.flt $< -Wl,-elf2flt=-r
	#../buildroot/output/host/bin/riscv32-linux-objdump -S hello_linux.elf > hello_linux.S
	#../buildroot/output/host/bin/riscv32-linux-elf2flt -r -o hello_linux hello_linux.elf

clean :
	rm -rf hello_linux.flt hello_linux.flt.gdb
