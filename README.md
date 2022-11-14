# riscv_emufun

## Processor type

rv32ima/su+Zifencei+Zicsr


## Prereq

For bare metal:
```
sudo apt-get install gcc-multilib gcc-riscv64-unknown-elf make
```

## Buildroot Notes

Neat tools:

In root:
`make toolchain`

From `buildroot-2022.02.6`:
`make linux-menuconfig`
`make menuconfig`





## Building Tests

```
cd riscv-tests
export CROSS_COMPILE=riscv64-linux-gnu-
export PLATFORM_RISCV_XLEN=32
CC=riscv64-linux-gnu-gcc ./configure
make XLEN=32 RISCV_PREFIX=riscv64-unknown-elf- RISCV_GCC_OPTS="-g -O1 -march=rv32imaf -mabi=ilp32f -I/usr/include"
```


## Building OpenSBI

```
cd opensbi
export CROSS_COMPILE=riscv64-unknown-elf-
export PLATFORM_RISCV_XLEN=32
make
```

## Resources

 * https://jborza.com/emulation/2020/04/09/riscv-environment.html
 * https://blog.pimaker.at/texts/rvc1/


