REPO := riscv-tests
PREFIX := ../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-
OBJCOPY := $(PREFIX)objcopy
EMULATOR := ../mini-rv32ima/mini-rv32ima
JOBS ?= 4
RISCV_GCC_OPTS ?= -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles -g -O1
GROUP ?= rv32mi
TARGET ?= $(GROUP)
TEST ?= $(GROUP)-p-breakpoint
TEST_ELF := $(REPO)/isa/$(TEST)
TEST_BIN := $(TEST_ELF).bin

all: run

$(REPO):
	git clone https://github.com/riscv-software-src/riscv-tests
	cd $(REPO) && git submodule update --init --recursive

configure: $(REPO)
	cd $(REPO) && ./configure --prefix=

build: configure
	cd $(REPO) && make -j$(JOBS) XLEN=32 RISCV_PREFIX=$(abspath $(PREFIX)) RISCV_GCC_OPTS="$(RISCV_GCC_OPTS)" -C isa $(TARGET)

$(TEST_BIN): $(TEST_ELF)
	$(OBJCOPY) $< -O binary $@

run: build $(TEST_BIN)
	$(EMULATOR) -f $(TEST_BIN) -c 20000000 -d

clean:
	rm -rf $(REPO)

.PHONY: all configure build run clean
