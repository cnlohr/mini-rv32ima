
.section .initial_jump
.global _start
.global asm_demo_func

.align 4
_start:
	la	sp, _sstack
	addi	sp,sp,-16
	sw	ra,12(sp)
	jal	ra, main


.section .data

.align 4
asm_label:
	.ascii "I'm an assembly function Woot Woot."
	.byte 0

.section .text

.align 4
asm_demo_func:
	addi sp, sp, -16
	sw t0, 12(sp)
	la t0, asm_label
	csrw 0x138, t0
	lw t0, 12(sp)
	addi sp, sp, 16
	ret

