

.section .initial_jump
.global _start
.global asm_demo_func

.align 4
_start:
	lui sp, 0x80100
	addi	sp,sp,-16
	sw	ra,12(sp)
	jal	ra, main

asm_label:
	.ascii "I'm an assembly function."

.align 4
asm_demo_func:
	la t0, asm_label
	csrw 0x138, t0
	ret



