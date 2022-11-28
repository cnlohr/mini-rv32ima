#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Western Digital Corporation or its affiliates.
#

# Compiler flags
platform-cppflags-y =
platform-cflags-y =
platform-asflags-y =
platform-ldflags-y =

# Objects to build
platform-objs-y += platform.o

PLATFORM_RISCV_XLEN = 32

# Blobs to build
FW_TEXT_START=0x00000000
FW_JUMP=y
FW_PAYLOAD_ALIGN=0x1000

ifeq ($(PLATFORM_RISCV_XLEN), 32)
 # This needs to be 4MB aligned for 32-bit support
 FW_JUMP_ADDR=0x00001000
else
 # This device doesn't support 64-bit.
endif

FW_PAYLOAD=y

