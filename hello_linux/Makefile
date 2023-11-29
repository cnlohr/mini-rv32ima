all : hello_linux pi

PREFIX:=../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-
CC:=$(PREFIX)gcc
OBJDUMP:=$(PREFIX)objdump

# Note:  regymm says to do -fPIE -pie -static, instead of -fPIC
CFLAGS:=-mabi=ilp32 -fPIE -pie -static -march=rv32ima -Os -s -g -ffunction-sections -fdata-sections
LDFLAGS:=-Wl,-elf2flt=-r -Wl,-gc-sections

C_S+=hello_linux.c

test : deploy
	make -C .. toolchain
	make -C ../mini-rv32ima mini-rv32ima
	../mini-rv32ima/mini-rv32ima -f ../buildroot/output/images/Image

hello_linux : hello_linux.c
	$(CC) $(CFLAGS) $(C_S) $(LDFLAGS) -o $@
	$(OBJDUMP) -S $@.gdb > $@.S

pi : pi.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

deploy : hello_linux pi
	cp $^ ../buildroot/output/target/root

rebuild :
	make -C.. toolchain 

clean :
	rm -rf hello_linux hello_linux.gdb hello_linux.S pi pi.gdb pi.S
