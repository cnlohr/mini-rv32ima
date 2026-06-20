

#include <stdint.h>
#ifndef XLEN
#define XLEN 32
#elif (XLEN != 32) || (XLEN != 64)
#define XLEN 32
#endif

#if (XLEN == 32)
typedef uint32_t xlen_t;
typedef int32_t sxlen_t;
typedef uint64_t xlen2_t;
typedef int64_t sxlen2_t;
#define SXLEN_MIN INT32_MIN
#define XLEN_MAX UINT32_MAX
#else
typedef uint64_t xlen_t;
typedef int64_t sxlen_t;
typedef uint128_t xlen2_t;
typedef int128_t sxlen2_t;
#define SXLEN_MIN INT64_MIN
#define XLEN_MAX UINT64_MAX
#endif

#define MSTATUS_MIE_BIT 3
#define MSTATUS_MPIE_BIT 7
#define MSTATUS_MPP_START_BIT 11

#define MCAUSE_INTERRUPT_BIT (XLEN-1)
#define MCAUSE_INSTRUCTION_ADDR_MISALIGNED 0
#define MCAUSE_INSTRUCTION_ACCESS_FAULT 1
#define MCAUSE_ILLEGAL_INSTRUCTION 2
#define MCAUSE_BREAKPOINT 3
#define MCAUSE_LOAD_ADDR_MISALIGNED 4
#define MCAUSE_LOAD_ACCESS_FAULT 5
#define MCAUSE_STORE_ADDR_MISALIGNED 6
#define MCAUSE_STORE_ACCESS_FAULT 7
#define MCAUSE_ECALL_FROM_U 8
#define MCAUSE_ECALL_FROM_S 9
#define MCAUSE_ECALL_FROM_M 11

#define MIP_MTIP_BIT 7

#define EXTRAFLAGS_WFI_BIT 2

#define GET_RD(ir) ((ir >> 7) & 0x1f)
#define GET_RS1(ir) ( (ir >> 15) & 0x1f )
#define GET_RS2(ir) ( (ir >> 20) & 0x1f )
#define GET_OPCODE(ir) (ir & 0x7f)
#define GET_FUNC3(ir) ( ( ir >> 12 ) & 0x7 )
#define GET_CSR_ADDR(ir) (ir >> 20)
#if (XLEN == 64)
#define UTYPE_IMM(ir) ((uint64_t)((int32_t)( ir & 0xfffff000 )))
#define JTYPE_IMM(ir) ((uint64_t)((int32_t)(((uint32_t)(((int32_t)(ir & 0x80000000))>>11)) | ((ir & 0x7fe00000)>>20) | ((ir & 0x00100000)>>9) | ((ir&0x000ff000)))))
#define ITYPE_IMM(ir) ((uint64_t)(((int32_t)ir) >> 20))
#define BTYPE_IMM(ir) ((uint64_t)((int32_t)(((ir & 0xf00)>>7) | ((ir & 0x7e000000)>>20) | ((ir & 0x80) << 4) | ((uint32_t)((((int32_t)ir) >> 31)<<12)))))
#define STYPE_IMM(ir) ((uint64_t)((int32_t)(( ( ir >> 7 ) & 0x1f ) | ((uint32_t)( ((int32_t)( ir & 0xfe000000 )) >> 20 )))))
#else
#define UTYPE_IMM(ir) ( ir & 0xfffff000 )
#define JTYPE_IMM(ir) (((uint32_t)(((int32_t)(ir & 0x80000000))>>11)) | ((ir & 0x7fe00000)>>20) | ((ir & 0x00100000)>>9) | ((ir&0x000ff000)))
#define ITYPE_IMM(ir) ((uint32_t)(((int32_t)ir) >> 20))
#define BTYPE_IMM(ir) (((ir & 0xf00)>>7) | ((ir & 0x7e000000)>>20) | ((ir & 0x80) << 4) | ((uint32_t)((((int32_t)ir) >> 31)<<12)))
#define STYPE_IMM(ir) (( ( ir >> 7 ) & 0x1f ) | ((uint32_t)( ((int32_t)( ir & 0xfe000000 )) >> 20 )))
#endif

#define OPCODE_LUI 0b0110111
#define OPCODE_AUIPC 0b0010111
#define OPCODE_JAL 0b1101111
#define OPCODE_JALR 0b1100111
#define OPCODE_BRANCH 0b1100011
#define OPCODE_LOAD 0b0000011
#define OPCODE_STORE 0b0100011
#define OPCODE_OP 0b0110011
#define OPCODE_OP_IMM 0b0010011
#define OPCODE_MISC_MEM 0b0001111
#define OPCODE_SYSTEM 0b1110011
#define OPCODE_OP_32 0b0111011
#define OPCODE_OP_IMM_32 0b0011011

#define IS_FUNC7_MUL(ir) ( ir & 0x02000000 )
#define IS_FUNC7_SUB(ir) ( ir & 0x40000000 )
#define IS_FUNC7_SRA(ir) ( ir & 0x40000000 )
#if (XLEN==64)
#define SHIFT_MASK 0x3F
#else
#define SHIFT_MASK 0x1F
#endif

#if (XLEN==64)
#define MXL 2
#else
#define MXL 1
#endif

#define MISA_READ (0x40401101UL | (((xlen_t)MXL) << (XLEN-2)))

#if (XLEN==64)
#define SET_MCYCLE(x) SETCSR( cycle, x )
#else
#define SET_MCYCLE(x) if( CSR( cyclel ) > x ) { CSR( cycleh )++; } SETCSR( cyclel, x )
#endif

#define RESERVATION_MASK 0xFFFFFFFFFFFFFFF8UL