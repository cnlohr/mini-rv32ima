all : mini-rv32ima mini-rv32ima.flt

mini-rv32ima : mini-rv32ima.c mini-rv32ima.h default64mbdtc.h
	# for debug
	gcc -o $@ $< -g -O4 -Wall
	gcc -o $@.tiny $< -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -fwhole-program -s

mini-rv32ima.flt : mini-rv32ima.c mini-rv32ima.h
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-gcc  -march=rv32ima -mabi=ilp32 -fPIC $< -Wl,-elf2flt=-r -o $@
	## Deply with: make clean all && cp mini-rv32ima.flt* ../buildroot/output/target/root/ && make -C .. toolchain && make testkern

testkern : mini-rv32ima
	./mini-rv32ima -f ../buildroot/output/images/Image

testbare : mini-rv32ima
	./mini-rv32ima -f ../baremetal/baremetal.bin

DownloadedImage :
	wget https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/linux-5.18.0-rv32nommu-cnl-1.tar.bz2 -O linux-5.18.0-rv32nommu-cnl-1.tar.bz2
	tar xjvf linux-5.18.0-rv32nommu-cnl-1.tar.bz2
	mv Image DownloadedImage

testdlimage : mini-rv32ima DownloadedImage
	./mini-rv32ima -f DownloadedImage

# For dumping specific binaries and info.

# SBI is not currently working
#testsbi : mini-rv32ima
#	./mini-rv32ima -f ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin
#dumpsbi : 
#	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf >fw_payload.S


#bintoh : 
#	echo -ne "#include <stdio.h>\nint main( int argc, char ** argv ) { if( argc == 1 ) return -1; int c, p = 0; printf( \"static unsigned char %s[] = {\\n\", argv[1] ); while( ( c = getchar() ) != EOF ) printf( \"0x%02x,%c\", c, (((p++)&15)==15)?'\\n':' '); printf( \\"};\\n\" ); }" > bintoh.c
#	gcc bintoh.c -o bintoh
#sixtyfourmb.dtb.h : sixtyfourmb.dtb bintoh
#	./bintoh default64mbdtb < $< > $@

dumpkern :
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.S
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -t ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.t

clean :
	rm -rf mini-rv32ima mini-rv32ima.flt

