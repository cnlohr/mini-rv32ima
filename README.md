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


