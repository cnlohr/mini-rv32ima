all :

toolchain : buildroot-2022.02.6
	cp buildroot-2022.02.6-config buildroot-2022.02.6/.config
	cp -a custom_kernel_config buildroot-2022.02.6/kernel_config
	cp riscv_Kconfig buildroot-2022.02.6/output/build/linux-5.15.67/arch/riscv/
	make -C buildroot-2022.02.6

opensbi_firmware : 
	make -C opensbi PLATFORM=../../this_opensbi/platform/riscv_emufun I=../this_opensbi/install B=../this_opensbi/build CROSS_COMPILE=riscv64-unknown-elf- PLATFORM_RISCV_XLEN=32

buildroot-2022.02.6 : 
	wget https://buildroot.org/downloads/buildroot-2022.02.6.tar.gz
	tar xzvpf buildroot-2022.02.6.tar.gz
	rm buildroot-2022.02.6.tar.gz


