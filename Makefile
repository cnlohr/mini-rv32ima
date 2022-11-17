all :

toolchain_buildrootb : buildroot-2022.02.6
	cp buildroot-2022.02.6-config buildroot-2022.02.6/.config
	cp -a custom_kernel_config buildroot-2022.02.6/kernel_config
	cp riscv_Kconfig buildroot-2022.02.6/output/build/linux-5.15.67/arch/riscv/
	make -C buildroot-2022.02.6

buildroot-2022.02.6_b : 
	wget https://buildroot.org/downloads/buildroot-2022.02.6.tar.gz
	tar xzvpf buildroot-2022.02.6.tar.gz
	rm buildroot-2022.02.6.tar.gz


opensbi_firmware : 
	make -C opensbi PLATFORM=../../this_opensbi/platform/riscv_emufun I=../this_opensbi/install B=../this_opensbi/build CROSS_COMPILE=riscv64-unknown-elf- PLATFORM_RISCV_ISA=rv32ima PLATFORM_RISCV_XLEN=32
	# ./mini-rv32ima -i ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin
	# ../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf > fw_payload.S


buildroot :
	git clone https://github.com/regymm/buildroot --recurse-submodules
	#git clone https://github.com/cnlohr/buildroot --recurse-submodules

toolchain : buildroot
	#cp buildroot-2022.02.6-config buildroot-2022.02.6/.config
	#cp -a custom_kernel_config buildroot-2022.02.6/kernel_config
	#cp riscv_Kconfig buildroot-2022.02.6/output/build/linux-5.15.67/arch/riscv/
	#cp -a custom_kernel_config buildroot/kernel_config
	#cp -a buildroot_config buildroot/.config
	#mkdir -p buildroot/board/riscv/nommu/patches
	make -C buildroot qemu_riscv32_nommu_virt_minimal_defconfig


