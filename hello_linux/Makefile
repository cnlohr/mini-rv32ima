all : hello_linux.flt

hello_linux.flt : hello_linux.c
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-gcc -march=rv32ima -mabi=ilp32 -fPIC hello_linux.c -Wl,-elf2flt=-r -o hello_linux.flt
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S hello_linux.flt.gdb > hello_linux.S

	#../buildroot/output/host/bin/riscv32-linux-gcc -march=rv32ima -mabi=ilp32 -fPIC -Wl,-elf2flt=-r hello_linux.c -o hello_linux.flt
	#../buildroot/output/host/bin/riscv32-linux-objdump -S hello_linux.flt.gdb > hello_linux.S
	#../buildroot/output/host/bin/riscv32-linux-objdump -S hello_linux.elf > hello_linux.S
	#../buildroot/output/host/bin/riscv32-linux-elf2flt -r -o hello_linux hello_linux.elf

clean :
	rm -rf hello_linux.flt hello_linux.flt.gdb