# riscv_emufun

## VERY EARLY ROUGH DO NOT USE

## Processor type

Probably/maybe something like: rv32ima/??su+Zifencei+Zicsr

Right now I am just using an rv32im.  But wouldn't it be cool if we could get buildroot running on that?

TODO:
 * Actually make OpenSBI work, it's what buildroot uses to load the Kernel.


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

## QEMU Test

```
sudo apt install qemu-system-misc
```

```
output/host/bin/qemu-system-riscv32 -cpu rv32,mmu=false -m 128M -machine virt -nographic -kernel output/images/Image -bios none -drive file=output/images/rootfs.ext2,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
output/host/bin/qemu-system-riscv32 -cpu rv32,mmu=false -m 128M -machine virt -nographic -kernel output/images/Image -bios none -drive file=output/images/rootfs.ext2,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -machine dumpdtb=../dtb.dtb
```

## Running from INITRD instead of DISK
 * In buildroot: Filesystem Images, check cpio, no compression.
 * In kernel: 

## Building Tests

(This does not work, now)
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


## General notes:
 * QEMU
 * Pi's Linux
 * https://github.com/cnlohr/riscv_emufun/commit/2f09cdeb378dc0215c07eb63f5a6fb43dbbf1871#diff-b48ccd795ae9aced07d022bf010bf9376232c4d78210c3113d90a8d349c59b3dL440
 * Making the kernel assembly to dig through.
 * touch kernel_config && make
 * Debugging process.



