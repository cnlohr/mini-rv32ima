all :

toolchain : buildroot-2022.02.6
	cp buildroot-2022.02.6-config buildroot-2022.02.6/.config
	mkdir -p buildroot-2022.02.6/output/build/linux-5.15.67/arch/riscv/configs
	cp -a kernel_config buildroot-2022.02.6/
	make -C buildroot-2022.02.6

buildroot-2022.02.6 : 
	wget https://buildroot.org/downloads/buildroot-2022.02.6.tar.gz
	tar xzvpf buildroot-2022.02.6.tar.gz
	rm buildroot-2022.02.6.tar.gz


