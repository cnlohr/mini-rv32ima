all : mini-rv32ima mini-rv32ima.flt

mini-rv32ima : mini-rv32ima.c mini-rv32ima.h default64mbdtc.h
	# for debug
	gcc -o $@ $< -g -O4 -Wall
	gcc -o $@.tiny $< -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -fwhole-program -s

mini-rv32ima.flt : mini-rv32ima.c mini-rv32ima.h
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-gcc -O4 -funroll-loops -s -march=rv32ima -mabi=ilp32 -fPIC $< -Wl,-elf2flt=-r -o $@

# Deply with:  make clean all && cp mini-rv32ima.flt ../buildroot/output/target/root/ && make -C .. toolchain && make testkern

profile : Image.ProfileTest
	echo "#!/bin/sh" > ../buildroot/output/target/etc/init.d/SLocal
	echo 'if [ "$$1" = "start" ]; then /root/mini-rv32ima.flt -f /root/Image.ProfileTest -plt 4 -m 0xfff000; poweroff; fi' >> ../buildroot/output/target/etc/init.d/SLocal
	chmod +x ../buildroot/output/target/etc/init.d/SLocal
	cp Image.ProfileTest ../buildroot/output/target/root/
	make clean all && cp mini-rv32ima.flt ../buildroot/output/target/root/ && make -C .. toolchain && ./mini-rv32ima -f ../buildroot/output/images/Image -plt 4 -m 0x4000000
	echo "Be sure to record the immediately above value."

Image.ProfileTest :
	wget https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/Image.ProfileTest-linux-5.18.0-rv32nommu.zip
	unzip Image.ProfileTest-linux-5.18.0-rv32nommu.zip


testkern : mini-rv32ima
	./mini-rv32ima -f ../buildroot/output/images/Image

testbare : mini-rv32ima
	./mini-rv32ima -f ../baremetal/baremetal.bin

DownloadedImage :
	wget https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/linux-5.18.0-rv32nommu-cnl-1.zip -O linux-5.18.0-rv32nommu-cnl-1.zip
	unzip linux-5.18.0-rv32nommu-cnl-1.zip
	mv Image DownloadedImage

testdlimage : mini-rv32ima DownloadedImage
	./mini-rv32ima -f DownloadedImage

# For dumping specific binaries and info.
# SBI is not currently working
#testsbi : mini-rv32ima
#	./mini-rv32ima -f ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.bin
#dumpsbi : 
#	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../opensbi/this_opensbi/platform/riscv_emufun/firmware/fw_payload.elf >fw_payload.S

# For converting the .dtb into a .h file for embeddding.
bintoh :
	echo "#include <stdio.h>" > bintoh.c
	echo "int main(int argc,char ** argv) {if(argc==1) return -1; int c, p=0; printf( \"static const unsigned char %s[] = {\", argv[1] ); while( ( c = getchar() ) != EOF ) printf( \"0x%02x,%c\", c, (((p++)&15)==15)?10:' '); printf( \"};\" ); return 0; }" >> bintoh.c
	gcc bintoh.c -o bintoh

default64mbdtc.h : sixtyfourmb.dtb bintoh
	./bintoh default64mbdtb < $< > $@
	# WARNING: sixtyfourmb.dtb MUST hvave at least 16 bytes of buffer room AND be 16-byte aligned.
	#  dtc -I dts -O dtb -o sixtyfourmb.dtb sixtyfourmb.dts -S 1536

dumpkern :
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -S ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.S
	../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump -t ../buildroot/output/build/linux-5.18/vmlinux >fw_payload.t

clean :
	rm -rf mini-rv32ima mini-rv32ima.flt

