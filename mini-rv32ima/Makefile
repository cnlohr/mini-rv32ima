all : mini-rv32ima mini-rv32ima.flt

mini-rv32ima : mini-rv32ima.c mini-rv32ima.h
	# for debug
	gcc -o $@ $< -g -Os -Wall
	gcc -o $@.tiny $< -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -fwhole-program -s

mini-rv32ima.flt : mini-rv32ima.c mini-rv32ima.h
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-gcc  -march=rv32ima -mabi=ilp32 -fPIC $< -Wl,-elf2flt=-r -o $@
	## Deply with: make clean all && cp mini-rv32ima.flt* ../buildroot/output/target/root/ && make -C .. toolchain && make testkern

testkern : mini-rv32ima
	./mini-rv32ima -b sixtyfourmb.dtb -f ../buildroot/output/images/Image

testbare : mini-rv32ima
	./mini-rv32ima -f ../baremetal/baremetal.bin

# For dumping specific binaries and info.

# SBI is not currently working
#testsbi : mini-rv32ima
#	./mini-rv32ima -f ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin
#dumpsbi : 
#	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf >fw_payload.S

dumpkern :
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.S
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -t ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.t

clean :
	rm -rf mini-rv32ima mini-rv32ima.flt

