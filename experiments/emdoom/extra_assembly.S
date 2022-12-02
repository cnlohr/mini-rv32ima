#if 0

// Left as a tombstone to a bug having to do with mulh (signed)

#ifndef GENERATE_BAKED
.global FixedMul
FixedMul:
	li a5, 0xAAAAAAAA
	csrw 0x137, a0
	csrw 0x137, a1
	csrw 0x137, a5
	mul	a5,a0,a1
	csrw 0x137, a5
	mulh a0,a0,a1
	csrw 0x137, a0
	srli a5,a5,0x10
	csrw 0x137, a5
	slli a0,a0,0x10
	csrw 0x137, a0
	or	 a0,a0,a5
	csrw 0x137, a0
	li a5, 0xBBBBBBBB
	csrw 0x137, a5
	ret
#endif

//	    pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
//                           -135168  29835 ( 1955205025 )   
// fffdf000,0000748b aaaaaaaa 0fa15000 0000748a {0fa15000 16 =00000fa1} 0000748a 748a0000 748a0fa1 bbbbbbbb
// fffdf000,0000748b aaaaaaaa 0fa15000 0000748a {0fa15000 16 =00000fa1} 0000748a 748a0000 748a0fa1 bbbbbbbb
// 0000748b 1055 =00000000
// FRACBITS == 16

// =00004bdf}000000040004000000044bdfbbbbbbbb{00540000 1028 =00054000}000000e200097000aaaaaaaa0854e00000000000{0854e000 16 =00000854}000000000000000000000854bbbbbbbb



//{00540000 1028 =00054000}000000e2fffdf000aaaaaaaafe2de000000000e1{fe2de000 16 =0000fe2d}000000e100e1000000e1fe2dbbbbbbbb{00000000 1025 =00000000}{00000000 1025 =00000000}{00000000 1025 =00000000}{00000000 1025 =00000000}{00000000 1025 =00000000}{00000000 1025 =00000000}{8b7b305f 1055 =ffffffff}{00000000 1025 =00000000}{00540000 1055 =00000000}{00000000 1025 =00000000}{fffdf000 1055 =ffffffff}{00000000 1025 =00000000}{0000748b 1055 =00000000}{00000000 1025 =00000000}{748a0fa1 1055 =00000000}{0000c9da 1036 =0000000c}{8b7bdf30 1036 =fff8b7bd}{64080000 19 =00000c81}000138ab011fc08caaaaaaaa72e03d840000015f{72e03d84 16



//fffde679 007fff70 aaaaaaaa 3dae5bf0 007ffe63 {3dae5bf0 16 =00003dae} 007ffe63 fe630000 fe633dae bbbbbbbb

/*
    return ((long long) a * (long long) b) >> FRACBITS;
    9054:	02b507b3          	mul	a5,a0,a1
    9058:	02b51533          	mulh	a0,a0,a1

0000905c <.LVL1>:
    905c:	0107d793          	srli	a5,a5,0x10
    9060:	01051513          	slli	a0,a0,0x10
}
    9064:	00f56533          	or	a0,a0,a5
    9068:	00008067          	ret
*/

#endif

