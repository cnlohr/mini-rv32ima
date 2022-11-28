EXE:=coremark

all : $(EXE)

CC:=../../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-gcc
OBJDUMP:=../../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-objdump

CFLAGS?=-march=rv32ima -mabi=ilp32 -fPIE -pie -Os -s -static
LDFLAGS?=-Wl,-elf2flt="-r -s 32768"

APPFOLDER:=coremark-1.01

CFLAGS+=-I$(APPFOLDER) -DPERFORMANCE_RUN=1 -I$(APPFOLDER)/linux
C_S+=$(APPFOLDER)/linux/core_portme.c
C_S+=$(APPFOLDER)/core_list_join.c
C_S+=$(APPFOLDER)/core_main.c
C_S+=$(APPFOLDER)/core_matrix.c
C_S+=$(APPFOLDER)/core_state.c
C_S+=$(APPFOLDER)/core_util.c

$(EXE) : $(APPFOLDER)
	$(CC) $(C_S) $(CFLAGS) -DFLAGS_STR="\"$(CFLAGS)\"" $(LDFLAGS) -o $@
	$(OBJDUMP) -S $@.gdb > $@.S

deploy : $(EXE)
	cp $^ ../../buildroot/output/target/root

rebuild : deploy
	make -C.. toolchain 

# sudo apt-get install node node-js-yaml node-yamlish python-yaml

$(APPFOLDER) :
	wget https://github.com/eembc/coremark/archive/refs/tags/v1.01.zip -O v1.01.zip
	unzip v1.01.zip

clean :
	rm -rf $(EXE) $(EXE).gdb $(EXE).S


