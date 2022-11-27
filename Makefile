all : everything

DTC:=buildroot/output/host/bin/dtc

buildroot :
	git clone https://github.com/regymm/buildroot --recurse-submodules

toolchain : buildroot
	cp -a custom_kernel_config buildroot/kernel_config
	cp -a buildroot_config buildroot/.config
	cp -a busybox_config buildroot/busybox_config
	make -C buildroot

subprojects :

everything : toolchain
	make -C hello_linux deploy
	make -C duktapetest deploy
	make -C coremark deploy
	make -C buildroot
	make -C mini-rv32ima testkern

##################################################################
# For Debugging 
####

# SBI doesn't work for some reason?
#opensbi_firmware : 
#	make -C opensbi PLATFORM=../../this_opensbi/platform/riscv_emufun I=../this_opensbi/install B=../this_opensbi/build CROSS_COMPILE=riscv64-unknown-elf- PLATFORM_RISCV_ISA=rv32ima PLATFORM_RISCV_XLEN=32
#	# ./mini-rv32ima -i ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin
#	# ../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf > fw_payload.S

#toolchain_buildrootb : buildroot-2022.02.6
#	cp buildroot-2022.02.6-config buildroot-2022.02.6/.config
#	cp -a custom_kernel_config buildroot-2022.02.6/kernel_config
#	cp riscv_Kconfig buildroot-2022.02.6/output/build/linux-5.15.67/arch/riscv/
#	make -C buildroot-2022.02.6

minimal.dtb : minimal.dts $(DTC)
	$(DTC) -I dts -O dtb -o minimal.dtb minimal.dts -S 2048

# Trick for extracting the DTB from 
dtbextract : $(DTC)
	# Need 	sudo apt  install device-tree-compiler
	cd buildroot && output/host/bin/qemu-system-riscv32 -cpu rv32,mmu=false -m 128M -machine virt -nographic -kernel output/images/Image -bios none -drive file=output/images/rootfs.ext2,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -machine dumpdtb=../dtb.dtb && cd ..
	$(DTC) -I dtb -O dts -o dtb.dts dtb.dtb

test_minimaldtb :
	cd buildroot && output/host/bin/qemu-system-riscv32 -cpu rv32,mmu=false -m 128M -machine virt -machine dtb=../minimal.dtb -nographic -kernel output/images/Image -bios none

tests :
	git clone https://github.com/riscv-software-src/riscv-tests
	./configure --prefix=
