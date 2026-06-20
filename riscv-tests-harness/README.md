# riscv-tests harness

This folder provides a lightweight wrapper for building and running
`riscv-tests` using the existing workspace toolchain and `mini-rv32ima`.

By default the harness builds the `rv32mi` group and runs the
`rv32mi-p-simple` test.

Usage:

  make -C riscv-tests-harness build
  make -C riscv-tests-harness run
  make -C riscv-tests-harness run TEST=rv32mi-p-add
  make -C riscv-tests-harness run GROUP=rv32mi TEST=rv32mi-p-add
  make -C riscv-tests-harness clean

The root `Makefile` also exposes `make tests` to run the default riscv-test
case.

Note: `mini-rv32ima` loads a raw binary image and does not automatically
exit on `tohost` writes, so this harness runs with a fixed instruction count
and uses the emulator's fail-on-fault mode.
