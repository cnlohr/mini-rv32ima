// Copyright 2022 Charles Lohr, you may use this file or any portions herein under any of the BSD, MIT, or CC0 licenses.

#ifndef _MINI_RV32IMAH_H
#define _MINI_RV32IMAH_H

#include "spec.h"
/**
    To use mini-rv32ima.h for the bare minimum, the following:

	#define MINI_RV32_RAM_SIZE ram_amt
	#define MINIRV32_IMPLEMENTATION

	#include "mini-rv32ima.h"

	Though, that's not _that_ interesting. You probably want I/O!


	Notes:
		* There is a dedicated CLNT at 0x10000000.
		* There is free MMIO from there to 0x12000000.
		* You can put things like a UART, or whatever there.
		* Feel free to override any of the functionality with macros.
*/

#ifndef MINIRV32WARN
	#define MINIRV32WARN( x... );
#endif

#ifndef MINIRV32_DECORATE
	#define MINIRV32_DECORATE static
#endif

#ifndef MINIRV32_RAM_IMAGE_OFFSET
	#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000UL
#endif

#ifndef MINIRV32_MMIO_RANGE
	#define MINIRV32_MMIO_RANGE(n)  (0x10000000 <= (n) && (n) < 0x12000000)
#endif

#ifndef MINIRV32_POSTEXEC
	#define MINIRV32_POSTEXEC(...);
#endif

#ifndef MINIRV32_HANDLE_MEM_STORE_CONTROL
	#define MINIRV32_HANDLE_MEM_STORE_CONTROL(...);
#endif

#ifndef MINIRV32_HANDLE_MEM_LOAD_CONTROL
	#define MINIRV32_HANDLE_MEM_LOAD_CONTROL(...);
#endif

#ifndef MINIRV32_OTHERCSR_WRITE
	#define MINIRV32_OTHERCSR_WRITE(...);
#endif

#ifndef MINIRV32_OTHERCSR_READ
	#define MINIRV32_OTHERCSR_READ(...);
#endif

#ifndef MINIRV32_CUSTOM_MEMORY_BUS
	#define MINIRV32_STORE8( ofs, val ) *(uint64_t*)(image + ofs) = val
	#define MINIRV32_STORE4( ofs, val ) *(uint32_t*)(image + ofs) = val
	#define MINIRV32_STORE2( ofs, val ) *(uint16_t*)(image + ofs) = val
	#define MINIRV32_STORE1( ofs, val ) *(uint8_t*)(image + ofs) = val
	#define MINIRV32_LOAD8( ofs ) *(uint64_t*)(image + ofs)
	#define MINIRV32_LOAD4( ofs ) *(uint32_t*)(image + ofs)
	#define MINIRV32_LOAD2( ofs ) *(uint16_t*)(image + ofs)
	#define MINIRV32_LOAD1( ofs ) *(uint8_t*)(image + ofs)
	#define MINIRV32_LOAD4_SIGNED( ofs ) *(int32_t*)(image + ofs)
	#define MINIRV32_LOAD2_SIGNED( ofs ) *(int16_t*)(image + ofs)
	#define MINIRV32_LOAD1_SIGNED( ofs ) *(int8_t*)(image + ofs)
#endif

// As a note: We quouple-ify these, because in HLSL, we will be operating with
// uint4's.  We are going to uint4 data to/from system RAM.
//
// We're going to try to keep the full processor state to 12 x uint4.
struct MiniRV32IMAState
{
	xlen_t regs[32];

	xlen_t pc;
	xlen_t mstatus;
#if (XLEN == 64)
	uint64_t cycle
#else
	uint32_t cyclel;
	uint32_t cycleh;
#endif

#if (XLEN == 64)
	uint64_t timer;
#else
	uint32_t timerl;
	uint32_t timerh;
#endif
#if (XLEN == 64)
	uint64_t timermatch
#else
	uint32_t timermatchl;
	uint32_t timermatchh;
#endif

	xlen_t mscratch;
	xlen_t mtvec;
	xlen_t mie;
	xlen_t mip;

	xlen_t mepc;
	xlen_t mtval;
	xlen_t mcause;

	// Note: only a few bits are used.  (Machine = 3, User = 0)
	// Bits 0..1 = privilege.
	// Bit 2 = WFI (Wait for interrupt)
	// Bit 3+ = Load/Store reservation LSBs.
	xlen_t extraflags;
};

#ifndef MINIRV32_STEPPROTO
MINIRV32_DECORATE int32_t MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t vProcAddress, uint32_t elapsedUs, int count );
#endif

#ifdef MINIRV32_IMPLEMENTATION

#ifndef MINIRV32_CUSTOM_INTERNALS
#define CSR( x ) state->x
#define SETCSR( x, val ) { state->x = val; }
#define REG( x ) state->regs[x]
#define REGSET( x, val ) { state->regs[x] = val; }
#endif

#ifndef MINIRV32_STEPPROTO
MINIRV32_DECORATE int32_t MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t vProcAddress, uint32_t elapsedUs, int count )
#else
MINIRV32_STEPPROTO
#endif
{
#if (XLEN == 64)
	CSR( timer ) = CSR( timer ) + elapsedUs;
#else
	uint32_t new_timer = CSR( timerl ) + elapsedUs;
	if( new_timer < CSR( timerl ) ) CSR( timerh )++;
	CSR( timerl ) = new_timer;
#endif

	// Handle Timer interrupt.
#if (XLEN == 64)
	if( ( CSR( timer ) > CSR( timermatch ) ) && ( CSR( timermatch ) ) )
#else
	if( ( CSR( timerh ) > CSR( timermatchh ) || ( CSR( timerh ) == CSR( timermatchh ) && CSR( timerl ) > CSR( timermatchl ) ) ) && ( CSR( timermatchh ) || CSR( timermatchl ) ) )
#endif
	{
		CSR( extraflags ) &= ~(1UL << EXTRAFLAGS_WFI_BIT); // Clear WFI
		CSR( mip ) |= (1UL << MIP_MTIP_BIT); //MTIP of MIP // https://stackoverflow.com/a/61916199/2926815  Fire interrupt.
	}
	else
		CSR( mip ) &= ~(1UL << MIP_MTIP_BIT);

	// If WFI, don't run processor.
	if( CSR( extraflags ) & (1UL << EXTRAFLAGS_WFI_BIT) )
		return 1;

	xlen_t trap = 0;
	xlen_t rval = 0;
	xlen_t pc = CSR( pc );
#if (XLEN == 64)
	uint64_t cycle = CSR( cycle );
#else
	uint32_t cycle = CSR( cyclel );
#endif

	if( ( CSR( mip ) & (1UL<<MIP_MTIP_BIT) ) && ( CSR( mie ) & (1UL<<MIP_MTIP_BIT) /*mtie*/ ) && ( CSR( mstatus ) & (1UL<<MSTATUS_MIE_BIT) /*mie*/) )
	{
		// Timer interrupt.
		trap = (1UL<<MCAUSE_INTERRUPT_BIT)|(MIP_MTIP_BIT);
		pc -= 4;
	}
	else // No timer interrupt?  Execute a bunch of instructions.
	for( int icount = 0; icount < count; icount++ )
	{
		xlen_t ir = 0;
		rval = 0;
		cycle++;
		xlen_t ofs_pc = pc - MINIRV32_RAM_IMAGE_OFFSET;

		if( ofs_pc >= MINI_RV32_RAM_SIZE )
		{
			trap = 1 + MCAUSE_INSTRUCTION_ACCESS_FAULT;  // Handle access violation on instruction read.
			break;
		}
		else if( ofs_pc & 3 )
		{
			trap = 1 + MCAUSE_INSTRUCTION_ADDR_MISALIGNED;  //Handle PC-misaligned access
			break;
		}
		else
		{
			ir = MINIRV32_LOAD4( ofs_pc );
			uint32_t rdid = GET_RD( ir );

			switch( GET_OPCODE( ir ) )
			{
				case OPCODE_LUI: // LUI (0b0110111)
					rval = UTYPE_IMM( ir );
					break;
				case OPCODE_AUIPC: // AUIPC (0b0010111)
					rval = pc + UTYPE_IMM( ir );
					break;
				case OPCODE_JAL: // JAL (0b1101111)
				{
					rval = pc + 4;
					pc = pc + JTYPE_IMM( ir ) - 4;
					break;
				}
				case OPCODE_JALR: // JALR (0b1100111)
				{
					rval = pc + 4;
					pc = (REG( GET_RS1( ir ) ) + ITYPE_IMM( ir )) - 4 /* pc is incremented by 4 later */;
					break;
				}
				case OPCODE_BRANCH: // Branch (0b1100011)
				{
					sxlen_t rs1 = REG( GET_RS1( ir ) );
					sxlen_t rs2 = REG( GET_RS2( ir ) );
					xlen_t immm4 = pc + BTYPE_IMM( ir ) - 4;
					rdid = 0;
					switch( GET_FUNC3(ir) )
					{
						// BEQ, BNE, BLT, BGE, BLTU, BGEU
						case 0: if( rs1 == rs2 ) pc = immm4; break;
						case 1: if( rs1 != rs2 ) pc = immm4; break;
						case 4: if( rs1 < rs2 ) pc = immm4; break;
						case 5: if( rs1 >= rs2 ) pc = immm4; break; //BGE
						case 6: if( (xlen_t)rs1 < (xlen_t)rs2 ) pc = immm4; break;   //BLTU
						case 7: if( (xlen_t)rs1 >= (xlen_t)rs2 ) pc = immm4; break;  //BGEU
						default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION);
					}
					break;
				}
				case OPCODE_LOAD: // Load (0b0000011)
				{
					xlen_t rs1 = REG( GET_RS1( ir ) );
					xlen_t rsval = rs1 + ITYPE_IMM( ir );

					rsval -= MINIRV32_RAM_IMAGE_OFFSET;
					if( rsval >= MINI_RV32_RAM_SIZE-3 )
					{
						rsval += MINIRV32_RAM_IMAGE_OFFSET;
						if( MINIRV32_MMIO_RANGE( rsval ) )  // UART, CLNT
						{
							MINIRV32_HANDLE_MEM_LOAD_CONTROL( rsval, rval );
						}
						else
						{
							trap = (1+MCAUSE_LOAD_ACCESS_FAULT);
							rval = rsval;
						}
					}
					else
					{
						switch( GET_FUNC3(ir) )
						{
							//LB, LH, LW, LD, LBU, LHU, LWU
							case 0: rval = MINIRV32_LOAD1_SIGNED( rsval ); break;
							case 1: rval = MINIRV32_LOAD2_SIGNED( rsval ); break;
							case 2: rval = MINIRV32_LOAD4_SIGNED( rsval ); break;
							case 4: rval = MINIRV32_LOAD1( rsval ); break;
							case 5: rval = MINIRV32_LOAD2( rsval ); break;
#if (XLEN == 64)
							case 3: rval = MINIRV32_LOAD8( rsval ); break;
							case 6: rval = MINIRV32_LOAD4( rsval ); break;
#endif
							default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION);
						}
					}
					break;
				}
				case OPCODE_STORE: // Store 0b0100011
				{
					xlen_t rs1 = REG( GET_RS1(ir) );
					xlen_t rs2 = REG( GET_RS2(ir) );
					xlen_t addy = STYPE_IMM(ir);
					addy += rs1 - MINIRV32_RAM_IMAGE_OFFSET;
					rdid = 0;

					if( addy >= MINI_RV32_RAM_SIZE-3 )
					{
						addy += MINIRV32_RAM_IMAGE_OFFSET;
						if( MINIRV32_MMIO_RANGE( addy ) )
						{
							MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, rs2 );
						}
						else
						{
							trap = (1+MCAUSE_STORE_ACCESS_FAULT); // Store access fault.
							rval = addy;
						}
					}
					else
					{
						switch( GET_FUNC3(ir) )
						{
							//SB, SH, SW, SD
							case 0: MINIRV32_STORE1( addy, rs2 ); break;
							case 1: MINIRV32_STORE2( addy, rs2 ); break;
							case 2: MINIRV32_STORE4( addy, rs2 ); break;
#if (XLEN==64)
							case 3: MINIRV32_STORE8( addy, rs2 ); break;
#endif
							default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION);
						}
					}
					break;
				}
				case OPCODE_OP_IMM: // Op-immediate 0b0010011
				case OPCODE_OP: // Op           0b0110011
				{
					xlen_t imm = ITYPE_IMM(ir);
					xlen_t rs1 = REG(GET_RS1(ir));
					xlen_t is_reg = !!( ir & 0x20 );
					xlen_t rs2 = is_reg ? REG(GET_RS2(ir)) : imm;

					if( is_reg && IS_FUNC7_MUL(ir) )
					{
						switch( GET_FUNC3(ir) ) //0x02000000 = RV32M
						{
							case 0: rval = rs1 * rs2; break; // MUL
#ifndef CUSTOM_MULH // If compiling on a system that doesn't natively, or via libgcc support 64-bit math.
							case 1: rval = ((sxlen2_t)((sxlen_t)rs1) * (sxlen2_t)((sxlen_t)rs2)) >> XLEN; break; // MULH
							case 2: rval = ((sxlen2_t)((sxlen_t)rs1) * (xlen2_t)rs2) >> XLEN; break; // MULHSU
							case 3: rval = ((xlen2_t)rs1 * (xlen2_t)rs2) >> XLEN; break; // MULHU
#else
							CUSTOM_MULH
#endif
							case 4: if( rs2 == 0 ) rval = -1L; else rval = ((sxlen_t)rs1 == SXLEN_MIN && (sxlen_t)rs2 == -1L) ? rs1 : ((sxlen_t)rs1 / (sxlen_t)rs2); break; // DIV
							case 5: if( rs2 == 0 ) rval = XLEN_MAX; else rval = rs1 / rs2; break; // DIVU
							case 6: if( rs2 == 0 ) rval = rs1; else rval = ((sxlen_t)rs1 == SXLEN_MIN && (sxlen_t)rs2 == -1L) ? 0 : ((xlen_t)((sxlen_t)rs1 % (sxlen_t)rs2)); break; // REM
							case 7: if( rs2 == 0 ) rval = rs1; else rval = rs1 % rs2; break; // REMU
						}
					}
					else
					{
						switch( GET_FUNC3(ir) ) // These could be either op-immediate or op commands.  Be careful.
						{
							case 0: rval = (is_reg && IS_FUNC7_SUB(ir) ) ? ( rs1 - rs2 ) : ( rs1 + rs2 ); break; 
							case 1: rval = rs1 << (rs2 & SHIFT_MASK); break;
							case 2: rval = (sxlen_t)rs1 < (sxlen_t)rs2; break;
							case 3: rval = rs1 < rs2; break;
							case 4: rval = rs1 ^ rs2; break;
							case 5: rval = IS_FUNC7_SRA(ir) ? ( ((sxlen_t)rs1) >> (rs2 & SHIFT_MASK) ) : ( rs1 >> (rs2 & SHIFT_MASK) ); break;
							case 6: rval = rs1 | rs2; break;
							case 7: rval = rs1 & rs2; break;
						}
					}
					break;
				}
#if (XLEN==64)
				case OPCODE_OP_IMM_32: // Op-32-immediate 0b0011011
				case OPCODE_OP_32: // Op-32          0b0111011
				{
					xlen_t imm = ITYPE_IMM(ir);
					xlen_t rs1 = REG(GET_RS1(ir));
					xlen_t is_reg = !!( ir & 0x20 );
					xlen_t rs2 = is_reg ? REG(GET_RS2(ir)) : imm;

					if( is_reg && IS_FUNC7_MUL(ir) )
					{
						switch( GET_FUNC3(ir) ) //0x02000000 = RV32M
						{
							case 0: rval = (int32_t) (((int32_t) rs1) * ((int32_t) rs2)); break; // MULW
							case 4: if( ((int32_t) rs2) == 0 ) rval = -1; else rval = ((int32_t)rs1 == INT32_MIN && (int32_t)rs2 == -1) ? ((int32_t)rs1) : ((int32_t)rs1 / (int32_t)rs2); break; // DIVW
							case 5: if( ((uint32_t)rs2) == 0 ) rval = UINT64_MAX; else rval = (int32_t)(((uint32_t)rs1) / ((uint32_t)rs2)); break; // DIVUW
							case 6: if( ((int32_t) rs2) == 0 ) rval = ((int32_t)rs1); else rval = ((int32_t)rs1 == INT32_MIN && (int32_t)rs2 == -1) ? 0 : ((int32_t)((uint32_t)((int32_t)rs1 % (int32_t)rs2))); break; // REMW
							case 7: if( ((uint32_t)rs2) == 0 ) rval = ((int32_t)rs1); else rval = (int32_t)(((uint32_t)rs1) % ((uint32_t)rs2)); break; // REMUW
							default: trap = 1+MCAUSE_ILLEGAL_INSTRUCTION;
						}
					}
					else
					{
						switch( GET_FUNC3(ir) ) // These could be either op-immediate or op commands.  Be careful.
						{
							case 0: rval = (int32_t)((is_reg && IS_FUNC7_SUB(ir) ) ? ( rs1 - rs2 ) : ( rs1 + rs2 )); break; 
							case 1: rval = (int32_t)(rs1 << (rs2 & 0x1F)); break;
							case 5: rval = (int32_t)(IS_FUNC7_SRA(ir) ? ( ((int32_t)rs1) >> (rs2 & 0x1F) ) : ( rs1 >> (rs2 & 0x1F) )); break;
							default: trap = 1+MCAUSE_ILLEGAL_INSTRUCTION;
						}
					}
					break;
				}
#endif
				case OPCODE_MISC_MEM: // 0b0001111
					rdid = 0;   // fencetype = (ir >> 12) & 0b111; We ignore fences in this impl.
					break;
				case OPCODE_SYSTEM: // Zifencei+Zicsr  (0b1110011)
				{
					uint32_t csrno = GET_CSR_ADDR(ir);
					uint32_t microop = GET_FUNC3(ir);
					if( (microop & 3) ) // It's a Zicsr function.
					{
						int rs1imm = GET_RS1(ir);
						xlen_t rs1 = REG(rs1imm);
						xlen_t writeval = rs1;

						// https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
						// Generally, support for Zicsr
						switch( csrno )
						{
						case 0x340: rval = CSR( mscratch ); break;
						case 0x305: rval = CSR( mtvec ); break;
						case 0x304: rval = CSR( mie ); break;
						case 0xC00: rval = cycle; break;
						case 0x344: rval = CSR( mip ); break;
						case 0x341: rval = CSR( mepc ); break;
						case 0x300: rval = CSR( mstatus ); break; //mstatus
						case 0x342: rval = CSR( mcause ); break;
						case 0x343: rval = CSR( mtval ); break;
						case 0xf11: rval = 0xff0ff0ff; break; //mvendorid
						case 0x301: rval = MISA_READ; break; //misa (XLEN=32, IMA+X)
						//case 0x3B0: rval = 0; break; //pmpaddr0
						//case 0x3a0: rval = 0; break; //pmpcfg0
						//case 0xf12: rval = 0x00000000; break; //marchid
						//case 0xf13: rval = 0x00000000; break; //mimpid
						//case 0xf14: rval = 0x00000000; break; //mhartid
						default:
							MINIRV32_OTHERCSR_READ( csrno, rval );
							break;
						}

						switch( microop )
						{
							case 1: writeval = rs1; break;  			//CSRRW
							case 2: writeval = rval | rs1; break;		//CSRRS
							case 3: writeval = rval & ~rs1; break;		//CSRRC
							case 5: writeval = rs1imm; break;			//CSRRWI
							case 6: writeval = rval | rs1imm; break;	//CSRRSI
							case 7: writeval = rval & ~rs1imm; break;	//CSRRCI
						}

						switch( csrno )
						{
						case 0x340: SETCSR( mscratch, writeval ); break;
						case 0x305: SETCSR( mtvec, writeval ); break;
						case 0x304: SETCSR( mie, writeval ); break;
						case 0x344: SETCSR( mip, writeval ); break;
						case 0x341: SETCSR( mepc, writeval ); break;
						case 0x300: SETCSR( mstatus, writeval ); break; //mstatus
						case 0x342: SETCSR( mcause, writeval ); break;
						case 0x343: SETCSR( mtval, writeval ); break;
						//case 0x3a0: break; //pmpcfg0
						//case 0x3B0: break; //pmpaddr0
						//case 0xf11: break; //mvendorid
						//case 0xf12: break; //marchid
						//case 0xf13: break; //mimpid
						//case 0xf14: break; //mhartid
						//case 0x301: break; //misa
						default:
							MINIRV32_OTHERCSR_WRITE( csrno, writeval );
							break;
						}
					}
					else if( microop == 0x0 ) // "SYSTEM" 0b000
					{
						rdid = 0;
						if( ( ( csrno & 0xff ) == 0x02 ) )  // MRET
						{
							//https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
							//Table 7.6. MRET then in mstatus/mstatush sets MPV=0, MPP=0, MIE=MPIE, and MPIE=1. La
							// Should also update mstatus to reflect correct mode.
							xlen_t startmstatus = CSR( mstatus );
							xlen_t startextraflags = CSR( extraflags );
							SETCSR( mstatus , (( startmstatus & (1UL<<MSTATUS_MPIE_BIT)) >> (MSTATUS_MPIE_BIT-MSTATUS_MIE_BIT)) | ((startextraflags&3) << MSTATUS_MPP_START_BIT) | (1UL<<MSTATUS_MPIE_BIT) );
							SETCSR( extraflags, (startextraflags & ~3) | ((startmstatus >> MSTATUS_MPP_START_BIT) & 3) );
							pc = CSR( mepc ) -4;
						} else {
							switch (csrno) {
							case 0:
								trap = ( CSR( extraflags ) & 3) ? (1+MCAUSE_ECALL_FROM_M) : (1+MCAUSE_ECALL_FROM_U); // ECALL; 8 = "Environment call from U-mode"; 11 = "Environment call from M-mode"
								break;
							case 1:
								trap = (1+MCAUSE_BREAKPOINT); break; // EBREAK 3 = "Breakpoint"
							case 0x105: //WFI (Wait for interrupts)
								CSR( mstatus ) |= (1UL << MSTATUS_MIE_BIT);    //Enable interrupts
								CSR( extraflags ) |= 4; //Infor environment we want to go to sleep.

								SET_MCYCLE( cycle );

								MINIRV32_POSTEXEC( pc, ir, trap );

								SETCSR( pc, pc + 4 );
								return 1;
							default:
								trap = (1+MCAUSE_ILLEGAL_INSTRUCTION); break; // Illegal opcode.
							}
						}
					}
					else
						trap = (1+MCAUSE_ILLEGAL_INSTRUCTION); 				// Note micrrop 0b100 == undefined.
					break;
				}
				case 0x2f: // RV32A (0b00101111)
				{
					xlen_t rs1 = REG(GET_RS1(ir));
					xlen_t rs2 = REG(GET_RS2(ir));
					uint32_t irmid = ( ir>>27 ) & 0x1f;

					rs1 -= MINIRV32_RAM_IMAGE_OFFSET;

					// We don't implement load/store from UART or CLNT with RV32A here.

					if( rs1 >= MINI_RV32_RAM_SIZE-3 )
					{
						trap = (1+MCAUSE_STORE_ACCESS_FAULT); //Store/AMO access fault
						rval = rs1 + MINIRV32_RAM_IMAGE_OFFSET;
					}
#if (XLEN == 64)
					elif (GET_FUNC3(ir) == 0b011)
					{
						rval = MINIRV32_LOAD8( rs1 );

						// Referenced a little bit of https://github.com/franzflasch/riscv_em/blob/master/src/core/core.c
						uint32_t dowrite = 1;
						switch( irmid )
						{
							case 2: //LR.D (0b00010)
								dowrite = 0;
								CSR( extraflags ) = (CSR( extraflags ) & (~RESERVATION_MASK)) | (rs1&RESERVATION_MASK);
								break;
							case 3:  //SC.D (0b00011) (Make sure we have a slot, and, it's valid)
								rval = ( (CSR( extraflags ) & RESERVATION_MASK) != ( rs1 & RESERVATION_MASK ) );  // Validate that our reservation slot is OK.
								(CSR( extraflags ) &= (~RESERVATION_MASK)); // reservation set in invalidated after each SC.D
								dowrite = !rval; // Only write if slot is valid.
								break;
							case 1: break; //AMOSWAP.D (0b00001)
							case 0: rs2 += rval; break; //AMOADD.D (0b00000)
							case 4: rs2 ^= rval; break; //AMOXOR.D (0b00100)
							case 12: rs2 &= rval; break; //AMOAND.D (0b01100)
							case 8: rs2 |= rval; break; //AMOOR.D (0b01000)
							case 16: rs2 = ((int64_t)rs2<(int64_t)rval)?rs2:rval; break; //AMOMIN.D (0b10000)
							case 20: rs2 = ((int64_t)rs2>(int64_t)rval)?rs2:rval; break; //AMOMAX.D (0b10100)
							case 24: rs2 = (rs2<rval)?rs2:rval; break; //AMOMINU.D (0b11000)
							case 28: rs2 = (rs2>rval)?rs2:rval; break; //AMOMAXU.D (0b11100)
							default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION); dowrite = 0; break; //Not supported.
						}
						if( dowrite ) MINIRV32_STORE8( rs1, rs2 );
					}
#endif
					else
					{
						rval = MINIRV32_LOAD4( rs1 );

						// Referenced a little bit of https://github.com/franzflasch/riscv_em/blob/master/src/core/core.c
						uint32_t dowrite = 1;
						switch( irmid )
						{
							case 2: //LR.W (0b00010)
								dowrite = 0;
								CSR( extraflags ) = (CSR( extraflags ) & (~RESERVATION_MASK)) | (rs1&RESERVATION_MASK);
								break;
							case 3:  //SC.W (0b00011) (Make sure we have a slot, and, it's valid)
								rval = ( (CSR( extraflags ) & RESERVATION_MASK) != ( rs1 & RESERVATION_MASK ) );  // Validate that our reservation slot is OK.
								(CSR( extraflags ) &= (~RESERVATION_MASK)); // reservation set in invalidated after each SC.W
								dowrite = !rval; // Only write if slot is valid.
								break;
							case 1: break; //AMOSWAP.W (0b00001)
							case 0: rs2 += rval; break; //AMOADD.W (0b00000)
							case 4: rs2 ^= rval; break; //AMOXOR.W (0b00100)
							case 12: rs2 &= rval; break; //AMOAND.W (0b01100)
							case 8: rs2 |= rval; break; //AMOOR.W (0b01000)
							case 16: rs2 = ((int32_t)rs2<(int32_t)rval)?rs2:rval; break; //AMOMIN.W (0b10000)
							case 20: rs2 = ((int32_t)rs2>(int32_t)rval)?rs2:rval; break; //AMOMAX.W (0b10100)
							case 24: rs2 = ((uint32_t)rs2<(uint32_t)rval)?rs2:rval; break; //AMOMINU.W (0b11000)
							case 28: rs2 = ((uint32_t)rs2>(uint32_t)rval)?rs2:rval; break; //AMOMAXU.W (0b11100)
							default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION); dowrite = 0; break; //Not supported.
						}
						if( dowrite ) MINIRV32_STORE4( rs1, rs2 );
					}
					break;
				}
				default: trap = (1+MCAUSE_ILLEGAL_INSTRUCTION); // Fault: Invalid opcode.
			}

			// If there was a trap, do NOT allow register writeback.
			if( trap ) {
				SETCSR( pc, pc );
				MINIRV32_POSTEXEC( pc, ir, trap );
				break;
			}

			if( rdid )
			{
				REGSET( rdid, rval ); // Write back register.
			}
		}

		MINIRV32_POSTEXEC( pc, ir, trap );

		pc += 4;
	}

	// Handle traps and interrupts.
	if( trap )
	{
		if( trap & 0x80000000 ) // If prefixed with 1 in MSB, it's an interrupt, not a trap.
		{
			SETCSR( mcause, trap );
			SETCSR( mtval, 0 );
			pc += 4; // PC needs to point to where the PC will return to.
		}
		else
		{
			SETCSR( mcause,  trap - 1 );
			SETCSR( mtval, (trap > 5 && trap <= 8)? rval : pc );
		}
		SETCSR( mepc, pc ); //TRICKY: The kernel advances mepc automatically.
		//CSR( mstatus ) & 8 = MIE, & 0x80 = MPIE
		// On an interrupt, the system moves current MIE into MPIE
		SETCSR( mstatus, (( CSR( mstatus ) & 0x08) << 4) | (( CSR( extraflags ) & 3 ) << 11) );
		pc = (CSR( mtvec ) - 4);

		// If trapping, always enter machine mode.
		CSR( extraflags ) |= 3;

		trap = 0;
		pc += 4;
	}

	SET_MCYCLE(cycle);
	SETCSR( pc, pc );
	return 0;
}

#endif

#endif


