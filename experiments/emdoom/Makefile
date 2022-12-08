all : build

#CFLAGS_EXTRA:=-DTERM256

test : build
	cp embeddeddoom/src/emdoom ../../buildroot/output/target/root
	make -C ../.. toolchain
	make -C ../../mini-rv32ima mini-rv32ima
	../../mini-rv32ima/mini-rv32ima -f ../../buildroot/output/images/Image -m 0x4000000

launch : 
	../../mini-rv32ima/mini-rv32ima -f ../../buildroot/output/images/Image -m 0x4000000

build : embeddeddoom
	CC=../../../../buildroot/output/host/usr/bin/riscv32-buildroot-linux-uclibc-gcc CFLAGS="-mabi=ilp32 -fPIE -pie -flto" CFLAGS_FINAL="-mabi=ilp32 -flto -fPIE -pie -static -march=rv32ima -O4 -s -g -DNORMALUNIX -DLINUX $(CFLAGS_EXTRA) -DMAXPLAYERS=1 -DDISABLE_NETWORK -static-libgcc -DSET_MEMORY_DEBUG=0 -DRANGECHECK -fdata-sections -ffunction-sections" LDFLAGS='-Wl,-elf2flt="-r -s 36000" -static -Wl,-Map=../../output.map -Wl,--gc-sections -flto' LIBS="" CS="../../video_console.c ../../extra_assembly.S ../../tmux_colour.c" make -C embeddeddoom/src
	../../buildroot/output/host/usr/bin/riscv32-buildroot-linux-uclibc-size -A embeddeddoom/src/emdoom.gdb
	../../buildroot/output/host/usr/bin/riscv32-buildroot-linux-uclibc-objdump -S embeddeddoom/src/emdoom.gdb > emdoom_listing.txt

#
#   -DRANGECHECK


# Only for desktop testing, almost never use.
desktop : embeddeddoom
	CFLAGS="-m32" CFLAGS_FINAL="-m32 -DRANGECHECK -DNORMALUNIX -DLINUX $(CFLAGS_EXTRA) -DMAXPLAYERS=1 -DDISABLE_NETWORK -DSET_MEMORY_DEBUG=0 -DIS_ON_DESKTOP_NOT_RV_EMULATOR -fsanitize=address -static-libasan -g -Wl,-Map=../../output.map " LDFLAGS='' LIBS="" CS="../../video_console.c ../../tmux_colour.c" make -C embeddeddoom/src

# If we were going to try to make a bare-metal version.
# -T ../../flatfile.lds
# ../../startup.S

# 

embeddeddoom :
	git clone https://github.com/cnlohr/embeddeddoom

clean :
	rm -rf *.o *~ output.map emdoom_listing.txt
	make -C embeddeddoom/src clean
