all : hello_linux

PREFIX:=../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-
CC:=$(PREFIX)gcc
OBJDUMP:=$(PREFIX)objdump

# Note:  regymm says to do -fPIE -pie -static, instead of -fPIC
CFLAGS:=-mabi=ilp32 -fPIE -pie -static -march=rv32ima -Os -s -g
LDFLAGS:=-Wl,-elf2flt=-r

C_S+=hello_linux.c

hello_linux : hello_linux.c
	$(CC) $(CFLAGS) $(C_S) $(LDFLAGS) -o $@
	$(OBJDUMP) -S $@.gdb > $@.S

deploy : hello_linux
	cp $^ ../buildroot/output/target/root

rebuild :
	make -C.. toolchain 

clean :
	rm -rf hello_linux hello_linux.gdb hello_linux.S
