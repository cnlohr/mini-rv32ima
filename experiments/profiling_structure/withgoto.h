// Copyright 2022 Charles Lohr, you may use this file or any portions herein under any of the BSD, MIT, or CC0 licenses.

#ifndef _MINI_RV32IMAH_H
#define _MINI_RV32IMAH_H

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
	#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000
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
	#define MINIRV32_STORE4( ofs, val ) *(uint32_t*)(image + ofs) = val
	#define MINIRV32_STORE2( ofs, val ) *(uint16_t*)(image + ofs) = val
	#define MINIRV32_STORE1( ofs, val ) *(uint8_t*)(image + ofs) = val
	#define MINIRV32_LOAD4( ofs ) *(uint32_t*)(image + ofs)
	#define MINIRV32_LOAD2( ofs ) *(uint16_t*)(image + ofs)
	#define MINIRV32_LOAD1( ofs ) *(uint8_t*)(image + ofs)
#endif

// As a note: We quouple-ify these, because in HLSL, we will be operating with
// uint4's.  We are going to uint4 data to/from system RAM.
//
// We're going to try to keep the full processor state to 12 x uint4.
struct MiniRV32IMAState
{
	uint32_t regs[32];

	uint32_t pc;
	uint32_t mstatus;
	uint32_t cyclel;
	uint32_t cycleh;

	uint32_t timerl;
	uint32_t timerh;
	uint32_t timermatchl;
	uint32_t timermatchh;

	uint32_t mscratch;
	uint32_t mtvec;
	uint32_t mie;
	uint32_t mip;

	uint32_t mepc;
	uint32_t mtval;
	uint32_t mcause;
	
	// Note: only a few bits are used.  (Machine = 3, User = 0)
	// Bits 0..1 = privilege.
	// Bit 2 = WFI (Wait for interrupt)
	uint32_t extraflags; 
};

MINIRV32_DECORATE int32_t MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t vProcAddress, uint32_t elapsedUs, int count );

#ifdef MINIRV32_IMPLEMENTATION

#define CSR( x ) state->x
#define SETCSR( x, val ) { state->x = val; }
#define REG( x ) state->regs[x]
#define REGSET( x, val ) { state->regs[x] = val; }

MINIRV32_DECORATE int32_t MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t vProcAddress, uint32_t elapsedUs, int count )
{
	uint32_t new_timer = CSR( timerl ) + elapsedUs;
	if( new_timer < CSR( timerl ) ) CSR( timerh )++;
	CSR( timerl ) = new_timer;

	// Handle Timer interrupt.
	if( ( CSR( timerh ) > CSR( timermatchh ) || ( CSR( timerh ) == CSR( timermatchh ) && CSR( timerl ) > CSR( timermatchl ) ) ) && ( CSR( timermatchh ) || CSR( timermatchl ) ) )
	{
		CSR( extraflags ) &= ~4; // Clear WFI
		CSR( mip ) |= 1<<7; //MSIP of MIP // https://stackoverflow.com/a/61916199/2926815  Fire interrupt.
	}
	else
		CSR( mip ) &= ~(1<<7);

	// If WFI, don't run processor.
	if( CSR( extraflags ) & 4 )
		return 1;

	int icount;

	uint32_t cyclel = CSR( cyclel );

	for( icount = 0; icount < count; icount++ )
	{
		uint32_t ir = 0;
		uint32_t trap = 0; // If positive, is a trap or interrupt.  If negative, is fatal error.
		uint32_t rval = 0;

		// Increment both wall-clock and instruction count time.  (NOTE: Not strictly needed to run Linux)
		if( ++cyclel == 0 ) CSR( cycleh )++;

		uint32_t pc = CSR( pc );
		uint32_t ofs_pc = pc - MINIRV32_RAM_IMAGE_OFFSET;

		if( ofs_pc >= MINI_RV32_RAM_SIZE ) { trap = 1; goto hittrap; } // Handle access violation on instruction read.
		if( ofs_pc & 3 ) { trap = 0; goto hittrap; } //Handle PC-misaligned access

		ir = MINIRV32_LOAD4( ofs_pc );
		uint32_t rdid = (ir >> 7) & 0x1f;

		switch( ir & 0x7f )
		{
			case 0b0110111: // LUI
				rval = ( ir & 0xfffff000 );
				break;
			case 0b0010111: // AUIPC
				rval = pc + ( ir & 0xfffff000 );
				break;
			case 0b1101111: // JAL
			{
				int32_t reladdy = ((ir & 0x80000000)>>11) | ((ir & 0x7fe00000)>>20) | ((ir & 0x00100000)>>9) | ((ir&0x000ff000));
				if( reladdy & 0x00100000 ) reladdy |= 0xffe00000; // Sign extension.
				rval = pc + 4;
				pc = pc + reladdy - 4;
				break;
			}
			case 0b1100111: // JALR
			{
				uint32_t imm = ir >> 20;
				int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
				rval = pc + 4;
				pc = ( (REG( (ir >> 15) & 0x1f ) + imm_se) & ~1) - 4;
				break;
			}
			case 0b1100011: // Branch
			{
				uint32_t immm4 = ((ir & 0xf00)>>7) | ((ir & 0x7e000000)>>20) | ((ir & 0x80) << 4) | ((ir >> 31)<<12);
				if( immm4 & 0x1000 ) immm4 |= 0xffffe000;
				int32_t rs1 = REG((ir >> 15) & 0x1f);
				int32_t rs2 = REG((ir >> 20) & 0x1f);
				immm4 = pc + immm4 - 4;
				rdid = 0;
				switch( ( ir >> 12 ) & 0x7 )
				{
					// BEQ, BNE, BLT, BGE, BLTU, BGEU 
					case 0b000: if( rs1 == rs2 ) pc = immm4; break;
					case 0b001: if( rs1 != rs2 ) pc = immm4; break;
					case 0b100: if( rs1 < rs2 ) pc = immm4; break;
					case 0b101: if( rs1 >= rs2 ) pc = immm4; break; //BGE
					case 0b110: if( (uint32_t)rs1 < (uint32_t)rs2 ) pc = immm4; break;   //BLTU
					case 0b111: if( (uint32_t)rs1 >= (uint32_t)rs2 ) pc = immm4; break;  //BGEU
					default: trap = (2); goto hittrap;
				}
				break;
			}
			case 0b0000011: // Load
			{
				uint32_t rs1 = REG((ir >> 15) & 0x1f);
				uint32_t imm = ir >> 20;
				int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
				uint32_t rsval = rs1 + imm_se;

				rsval -= MINIRV32_RAM_IMAGE_OFFSET;
				if( rsval >= MINI_RV32_RAM_SIZE-3 )
				{
					rsval += MINIRV32_RAM_IMAGE_OFFSET;
					if( rsval >= 0x10000000 && rsval < 0x12000000 )  // UART, CLNT
					{
						if( rsval == 0x1100bffc ) // https://chromitem-soc.readthedocs.io/en/latest/clint.html
							rval = CSR( timerh );
						else if( rsval == 0x1100bff8 )
							rval = CSR( timerl );
						else
							MINIRV32_HANDLE_MEM_LOAD_CONTROL( rsval, rval );
					}
					else
					{
						trap = (5);
						rval = rsval;
						goto hittrap;
					}
				}
				else
				{
					switch( ( ir >> 12 ) & 0x7 )
					{
						//LB, LH, LW, LBU, LHU
						case 0b000: rval = (int8_t)MINIRV32_LOAD1( rsval ); break;
						case 0b001: rval = (int16_t)MINIRV32_LOAD2( rsval ); break;
						case 0b010: rval = MINIRV32_LOAD4( rsval ); break;
						case 0b100: rval = MINIRV32_LOAD1( rsval ); break;
						case 0b101: rval = MINIRV32_LOAD2( rsval ); break;
						default: trap = (2); goto hittrap;
					}
				}
				break;
			}
			case 0b0100011: // Store
			{
				uint32_t rs1 = REG((ir >> 15) & 0x1f);
				uint32_t rs2 = REG((ir >> 20) & 0x1f);
				uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir & 0xfe000000 ) >> 20 );
				if( addy & 0x800 ) addy |= 0xfffff000;
				addy += rs1 - MINIRV32_RAM_IMAGE_OFFSET;
				rdid = 0;

				if( addy >= MINI_RV32_RAM_SIZE-3 )
				{
					addy += MINIRV32_RAM_IMAGE_OFFSET;
					if( addy >= 0x10000000 && addy < 0x12000000 ) 
					{
						// Should be stuff like SYSCON, 8250, CLNT
						if( addy == 0x11004004 ) //CLNT
							CSR( timermatchh ) = rs2;
						else if( addy == 0x11004000 ) //CLNT
							CSR( timermatchl ) = rs2;
						else if( addy == 0x11100000 ) //SYSCON (reboot, poweroff, etc.)
						{
							SETCSR( pc, pc + 4 );
							SETCSR( cyclel, cyclel );
							return rs2; // NOTE: PC will be PC of Syscon.
						}
						else
							MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, rs2 );
					}
					else
					{
						trap = (7); // Store access fault.
						rval = addy;
						goto hittrap;
					}
				}
				else
				{
					switch( ( ir >> 12 ) & 0x7 )
					{
						//SB, SH, SW
						case 0b000: MINIRV32_STORE1( addy, rs2 ); break;
						case 0b001: MINIRV32_STORE2( addy, rs2 ); break;
						case 0b010: MINIRV32_STORE4( addy, rs2 ); break;
						default: trap = (2); goto hittrap;
					}
				}
				break;
			}
			case 0b0010011: // Op-immediate
			case 0b0110011: // Op
			{
				uint32_t imm = ir >> 20;
				imm = imm | (( imm & 0x800 )?0xfffff000:0);
				uint32_t rs1 = REG((ir >> 15) & 0x1f);
				uint32_t is_reg = !!( ir & 0b100000 );
				uint32_t rs2 = is_reg ? REG(imm & 0x1f) : imm;

				if( is_reg && ( ir & 0x02000000 ) )
				{
					switch( (ir>>12)&7 ) //0x02000000 = RV32M
					{
						case 0b000: rval = rs1 * rs2; break; // MUL
						case 0b001: rval = ((int64_t)rs1 * (int64_t)rs2) >> 32; break; // MULH
						case 0b010: rval = ((int64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHSU
						case 0b011: rval = ((uint64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHU
						case 0b100: if( rs2 == 0 ) rval = -1; else rval = (int32_t)rs1 / (int32_t)rs2; break; // DIV
						case 0b101: if( rs2 == 0 ) rval = 0xffffffff; else rval = rs1 / rs2; break; // DIVU
						case 0b110: if( rs2 == 0 ) rval = rs1; else rval = (int32_t)rs1 % (int32_t)rs2; break; // REM
						case 0b111: if( rs2 == 0 ) rval = rs1; else rval = rs1 % rs2; break; // REMU
					}
				}
				else
				{
					switch( (ir>>12)&7 ) // These could be either op-immediate or op commands.  Be careful.
					{
						case 0b000: rval = (is_reg && (ir & 0x40000000) ) ? ( rs1 - rs2 ) : ( rs1 + rs2 ); break;
						case 0b001: rval = rs1 << rs2; break;
						case 0b010: rval = (int32_t)rs1 < (int32_t)rs2; break;
						case 0b011: rval = rs1 < rs2; break;
						case 0b100: rval = rs1 ^ rs2; break;
						case 0b101: rval = (ir & 0x40000000 ) ? ( ((int32_t)rs1) >> rs2 ) : ( rs1 >> rs2 ); break;
						case 0b110: rval = rs1 | rs2; break;
						case 0b111: rval = rs1 & rs2; break;
					}
				}
				break;
			}
			case 0b0001111: 
				rdid = 0;   // fencetype = (ir >> 12) & 0b111; We ignore fences in this impl.
				break;
			case 0b1110011: // Zifencei+Zicsr
			{
				uint32_t csrno = ir >> 20;
				int microop = ( ir >> 12 ) & 0b111;
				if( (microop & 3) ) // It's a Zicsr function.
				{
					int rs1imm = (ir >> 15) & 0x1f;
					uint32_t rs1 = REG(rs1imm);
					uint32_t writeval = rs1;

					// https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
					// Generally, support for Zicsr
					switch( csrno )
					{
					case 0x340: rval = CSR( mscratch ); break;
					case 0x305: rval = CSR( mtvec ); break;
					case 0x304: rval = CSR( mie ); break;
					case 0xC00: rval = CSR( cyclel ); break;
					case 0x344: rval = CSR( mip ); break;
					case 0x341: rval = CSR( mepc ); break;
					case 0x300: rval = CSR( mstatus ); break; //mstatus
					case 0x342: rval = CSR( mcause ); break;
					case 0x343: rval = CSR( mtval ); break;
					case 0xf11: rval = 0xff0ff0ff; break; //mvendorid
					case 0x301: rval = 0x40401101; break; //misa (XLEN=32, IMA+X)
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
						case 0b001: writeval = rs1; break;  			//CSRRW
						case 0b010: writeval = rval | rs1; break;		//CSRRS
						case 0b011: writeval = rval & ~rs1; break;		//CSRRC
						case 0b101: writeval = rs1imm; break;			//CSRRWI
						case 0b110: writeval = rval | rs1imm; break;	//CSRRSI
						case 0b111: writeval = rval & ~rs1imm; break;	//CSRRCI
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
				else if( microop == 0b000 ) // "SYSTEM"
				{
					rdid = 0;
					if( csrno == 0x105 ) //WFI (Wait for interrupts)
					{
						CSR( mstatus ) |= 8;    //Enable interrupts
						CSR( extraflags ) |= 4; //Infor environment we want to go to sleep.
						SETCSR( pc, pc + 4 );
						SETCSR( cyclel, cyclel );
						return 1;
					}
					else if( ( ( csrno & 0xff ) == 0x02 ) )  // MRET
					{
						//https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
						//Table 7.6. MRET then in mstatus/mstatush sets MPV=0, MPP=0, MIE=MPIE, and MPIE=1. La
						// Should also update mstatus to reflect correct mode.
						uint32_t startmstatus = CSR( mstatus );
						uint32_t startextraflags = CSR( extraflags );
						SETCSR( mstatus , (( startmstatus & 0x80) >> 4) | ((startextraflags&3) << 11) | 0x80 );
						SETCSR( extraflags, (startextraflags & ~3) | ((startmstatus >> 11) & 3) );
						pc = CSR( mepc ) -4;
					}
					else
					{
						switch( csrno )
						{
						case 0: trap = ( CSR( extraflags ) & 3) ? (11) : (8); break; // ECALL; 8 = "Environment call from U-mode"; 11 = "Environment call from M-mode"
						case 1:	trap = (3); break; // EBREAK 3 = "Breakpoint"
						default: trap = (2); break; // Illegal opcode.
						}
						goto hittrap;
					}
				}
				else
				{
					trap = (2); 				// Note micrrop 0b100 == undefined.
					goto hittrap;
				}
				break;
			}
			case 0b0101111: // RV32A
			{
				uint32_t rs1 = REG((ir >> 15) & 0x1f);
				uint32_t rs2 = REG((ir >> 20) & 0x1f);
				uint32_t irmid = ( ir>>27 ) & 0x1f;

				rs1 -= MINIRV32_RAM_IMAGE_OFFSET;

				// We don't implement load/store from UART or CLNT with RV32A here.

				if( rs1 >= MINI_RV32_RAM_SIZE-3 )
				{
					trap = (7); //Store/AMO access fault
					rval = rs1 + MINIRV32_RAM_IMAGE_OFFSET;
					goto hittrap;
				}
				else
				{
					rval = MINIRV32_LOAD4( rs1 );

					// Referenced a little bit of https://github.com/franzflasch/riscv_em/blob/master/src/core/core.c
					uint32_t dowrite = 1;
					switch( irmid )
					{
						case 0b00010: dowrite = 0; break; //LR.W
						case 0b00011: rval = 0; break; //SC.W (Lie and always say it's good)
						case 0b00001: break; //AMOSWAP.W
						case 0b00000: rs2 += rval; break; //AMOADD.W
						case 0b00100: rs2 ^= rval; break; //AMOXOR.W
						case 0b01100: rs2 &= rval; break; //AMOAND.W
						case 0b01000: rs2 |= rval; break; //AMOOR.W
						case 0b10000: rs2 = ((int32_t)rs2<(int32_t)rval)?rs2:rval; break; //AMOMIN.W
						case 0b10100: rs2 = ((int32_t)rs2>(int32_t)rval)?rs2:rval; break; //AMOMAX.W
						case 0b11000: rs2 = (rs2<rval)?rs2:rval; break; //AMOMINU.W
						case 0b11100: rs2 = (rs2>rval)?rs2:rval; break; //AMOMAXU.W
						default: trap = (2); goto hittrap; break; //Not supported.
					}
					if( dowrite ) MINIRV32_STORE4( rs1, rs2 );
				}
				break;
			}
			default: trap = (2); goto hittrap; // Fault: Invalid opcode.
		}

		if( rdid )
		{
			REGSET( rdid, rval );
		} // Write back register.
		else if( ( CSR( mip ) & (1<<7) ) && ( CSR( mie ) & (1<<7) /*mtie*/ ) && ( CSR( mstatus ) & 0x8 /*mie*/) )
		{
			trap = 0x80000007; goto hittrap; // Timer interrupt.
		}

		MINIRV32_POSTEXEC( pc, ir, trap );

		SETCSR( pc, pc + 4 );
		continue;
hittrap:
		MINIRV32_POSTEXEC( pc, ir, trap );

		{
			// Handle traps and interrupts.
			if( trap & 0x80000000 ) // If prefixed with 0x100, it's an interrupt, not a trap.
			{
				SETCSR( mcause, trap );
				SETCSR( mtval, 0 );
				pc += 4; // PC needs to point to where the PC will return to.
			}
			else
			{
				SETCSR( mcause,  trap );
				SETCSR( mtval, (trap > 5 && trap <= 8)? rval : pc );
			}
			SETCSR( mepc, pc ); //TRICKY: The kernel advances mepc automatically.
			//CSR( mstatus ) & 8 = MIE, & 0x80 = MPIE
			// On an interrupt, the system moves current MIE into MPIE
			SETCSR( mstatus, (( CSR( mstatus ) & 0x08) << 4) | (( CSR( extraflags ) & 3 ) << 11) );
			pc = (CSR( mtvec ) - 4);

			// XXX TODO: Do we actually want to check here? Is this correct?
			if( !(trap & 0x80000000) )
				CSR( extraflags ) |= 3;
		}
		SETCSR( pc, pc + 4 );
	}
	SETCSR( cyclel, cyclel );
	return 0;
}

#endif

#endif


