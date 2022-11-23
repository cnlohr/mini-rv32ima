all : mini-rv32ima

mini-rv32ima : mini-rv32ima.c
	gcc -o $@ $^ -lm -g

testsbi : mini-rv32ima
	./mini-rv32ima -f ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin

testkern : mini-rv32ima
	./mini-rv32ima -b ../minimal.dtb -f ../buildroot/output/images/Image

dumpsbi : 
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf >fw_payload.S

dumpkern :
	#../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -b binary -m riscv:rv32 -D ../buildroot/output/build/linux-5.18/arch/riscv/boot/Image >fw_payload.S
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.S
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -t ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.t

#../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump: supported targets: elf32-littleriscv elf64-littleriscv elf32-bigriscv elf64-bigriscv elf64-little elf64-big elf32-little elf32-big srec symbolsrec verilog tekhex binary ihex plugin
#../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump: supported architectures: riscv riscv:rv64 riscv:rv32




#### NOTE: Currently, we're getting stuck in void __noreturn sbi_init(struct sbi_scratch *scratch);  It looks like scratch->next_mode = something broken, because scratch = 0 it appaers?

clean :
	rm -rf mini-rv32ima

