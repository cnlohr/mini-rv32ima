
// Copyright 2022 Charles Lohr, you may use this file under any of the BSD, MIT, or CC0 licenses.

#ifndef _MINI_RV32IMAH_H
#define _MINI_RV32IMAH_H


// As a note: We quouple-ify these, because in HLSL, we will be operating with
// uint4's.  We are going to uint4 data to/from system RAM.
//
// We're going to try to keep the full processor state to 12 x uint4.
struct InternalCPUState
{
	uint32_t registers[32];

	uint32_t pc;
	uint32_t cycleh;
	uint32_t cyclel;
	uint32_t mstatus;

	uint32_t timerh;
	uint32_t timerl;
	uint32_t timermatchh;
	uint32_t timermatchl;

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


MINIRV32_DECORATE int StepInstruction( struct InternalCPUState * state, uint8_t * image, uint32_t vProcAddress, uint32_t elapsedUs )
{
	uint32_t * regs = state->registers;
	uint32_t retval = 0; // If positive, is a trap or interrupt.  If negative, is fatal error.

	// Increment both wall-clock and instruction count time.
	++state->cyclel;
	if( state->cyclel == 0 ) state->cycleh++;


	uint32_t new_timer = state->timerl + elapsedUs;
	if( new_timer < state->timerl ) state->timerh++;
	state->timerl = new_timer;

	// Handle Timer interrupt.
	if( ( state->timerh > state->timermatchh || ( state->timerh == state->timermatchh && state->timerl > state->timermatchl ) ) && ( state->timermatchh || state->timermatchl )  )
	{
		state->extraflags &= ~4; // Clear WFI
		state->mip |= 1<<7; //MSIP of MIP // https://stackoverflow.com/a/61916199/2926815  Fire interrupt.
	}
	else
		state->mip &= ~(1<<7);

	// If WFI, don't run processor.
	if( state->extraflags & 4 )
		return 1;

	uint32_t pc = state->pc;

	uint32_t ofs_pc = pc - MINIRV32_RAM_IMAGE_OFFSET;

	uint32_t ir = *(uint32_t*)(image + ofs_pc);

	int rdid = (ir >> 7) & 0x1f;
	uint32_t rval = 0;

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
			pc = ( (regs[ (ir >> 15) & 0x1f ] + imm_se) & ~1) - 4;
			break;
		}
		case 0b1100011: // Branch
		{
			uint32_t immm4 = ((ir & 0xf00)>>7) | ((ir & 0x7e000000)>>20) | ((ir & 0x80) << 4) | ((ir >> 31)<<12);
			if( immm4 & 0x1000 ) immm4 |= 0xffffe000;
			int32_t rs1 = regs[(ir >> 15) & 0x1f];
			int32_t rs2 = regs[(ir >> 20) & 0x1f];
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
				default: retval = (2+1);
			}
			break;
		}
		case 0b0000011: // Load
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t imm = ir >> 20;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t rsval = rs1 + imm_se;

			rsval -= MINIRV32_RAM_IMAGE_OFFSET;
			if( rsval >= MINI_RV32_RAM_SIZE-3 )
			{
				rsval -= MINIRV32_RAM_IMAGE_OFFSET;
				if( rsval >= 0x10000000 && rsval < 0x12000000 )  // UART, CLNT
				{
					int byteswaiting;
					char rxchar = 0;
					ioctl(0, FIONREAD, &byteswaiting);
					if( rsval == 0x10000005 )
						rval = 0x60 | !!byteswaiting;
					else if( rsval == 0x10000000 && byteswaiting )
					{
						if( read(fileno(stdin), (char*)&rxchar, 1) > 0 ) // Tricky: getchar can't be used with arrow keys.
							rval = rxchar;
					}
					else if( rsval == 0x1100bffc ) // https://chromitem-soc.readthedocs.io/en/latest/clint.html
						rval = state->timerh;
					else if( rsval == 0x1100bff8 )
						rval = state->timerl;
				}
				else
				{
					retval = (5+1);
					rval = rsval;
				}
			}
			else
			{
				switch( ( ir >> 12 ) & 0x7 )
				{
					//LB, LH, LW, LBU, LHU
					case 0b000: rval = *((int8_t*)(image + rsval)); break;
					case 0b001: rval = *((int16_t*)(image + rsval)); break;
					case 0b010: rval = *((uint32_t*)(image + rsval)); break;
					case 0b100: rval = *((uint8_t*)(image + rsval)); break;
					case 0b101: rval = *((uint16_t*)(image + rsval)); break;
					default: retval = (2+1);
				}
			}
			break;
		}
		case 0b0100011: // Store
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir & 0xfe000000 ) >> 20 );
			if( addy & 0x800 ) addy |= 0xfffff000;
			addy += rs1 - MINIRV32_RAM_IMAGE_OFFSET;
			rdid = 0;

			if( addy >= MINI_RV32_RAM_SIZE-3 )
			{
				addy -= MINIRV32_RAM_IMAGE_OFFSET;
				if( addy >= 0x10000000 && addy < 0x12000000 ) 
				{
					//Special: UART (8250)
					//If writing a byte, allow it to flow into output.
					if( addy == 0x10000000 )
					{
						printf( "%c", rs2 );
						fflush( stdout );
					}
					else if( addy == 0x11004004 ) //CLNT
						state->timermatchh = rs2;
					else if( addy == 0x11004000 ) //CLNT
						state->timermatchl = rs2;
					else if( addy == 0x11100000 ) //SYSCON
						return rs2;
				}
				else
				{
					retval = (7+1); // Store access fault.
					rval = addy + MINIRV32_RAM_IMAGE_OFFSET;
				}
			}
			else
			{
				switch( ( ir >> 12 ) & 0x7 )
				{
					//SB, SH, SW
					case 0b000: *((uint8_t*)(image + addy)) = rs2; break;
					case 0b001: *((uint16_t*)(image + addy)) = rs2; break;
					case 0b010: *((uint32_t*)(image + addy)) = rs2; break;
					default: retval = (2+1);
				}
			}
			break;
		}
		case 0b0010011: // Op-immediate
		case 0b0110011: // Op
		{
			uint32_t imm = ir >> 20;
			imm = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			int is_reg = ( ir & 0b100000 );
			uint32_t rs2 = is_reg ? regs[imm & 0x1f] : imm;

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
				uint32_t rs1 = regs[rs1imm];
				uint32_t writeval = rs1;

				// https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
				// Generally, support for Zicsr
				switch( csrno )
				{
				case 0x340: rval = state->mscratch; break;
				case 0x305: rval = state->mtvec; break;
				case 0x304: rval = state->mie; break;
				case 0xC00: rval = state->cyclel; break;
				case 0x344: rval = state->mip; break;
				case 0x341: rval = state->mepc; break;
				case 0x300: rval = state->mstatus; break; //mstatus
				case 0x342: rval = state->mcause; break;
				case 0x343: rval = state->mtval; break;
				case 0xf11: rval = 0xff0ff0ff; break; //mvendorid
				case 0x301: rval = 0x40001101; break; //misa (XLEN=32, IMA) TODO: Consider setting X bit.
				//case 0x3B0: rval = 0; break; //pmpaddr0
				//case 0x3a0: rval = 0; break; //pmpcfg0
				//case 0xf12: rval = 0x00000000; break; //marchid
				//case 0xf13: rval = 0x00000000; break; //mimpid
				//case 0xf14: rval = 0x00000000; break; //mhartid
				default:
					//MINIRV32WARN( "READ CSR: %08x\n", csrno );
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
				case 0x137: // Special, side-channel printf.
					MINIRV32WARN( "SIDE-CHANNEL-DEBUG: %s\n", image + writeval - MINIRV32_RAM_IMAGE_OFFSET );
					break;
				case 0x138: // Special, side-channel printf.
					MINIRV32WARN( "SIDE-CHANNEL-DEBUG: %08x\n", writeval );
					break;
				case 0x340: state->mscratch = writeval; break;
				case 0x305: state->mtvec = writeval; break;
				case 0x304: state->mie = writeval; break;
				case 0x344: state->mip = writeval; break;
				case 0x341: state->mepc = writeval; break;
				case 0x300: state->mstatus = writeval; break; //mstatus
				case 0x342: state->mcause = writeval; break;
				case 0x343: state->mtval = writeval; break;
				//case 0x3a0: break; //pmpcfg0
				//case 0x3B0: break; //pmpaddr0
				//case 0xf11: break; //mvendorid
				//case 0xf12: break; //marchid
				//case 0xf13: break; //mimpid
				//case 0xf14: break; //mhartid
				//case 0x301: break; //misa
				//default:
				//	MINIRV32WARN( "WRITE CSR: %08x = %08x\n", csrno, writeval );
				}
			}
			else if( microop == 0b000 ) // "SYSTEM"
			{
				rdid = 0;
				if( csrno == 0x105 ) //WFI
				{
					state->mstatus |= 8;
					state->extraflags |= 4;
				}
				else if( ( ( csrno & 0xff ) == 0x02 ) )  // MRET
				{
					//https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
					//Table 7.6. MRET then in mstatus/mstatush sets MPV=0, MPP=0, MIE=MPIE, and MPIE=1. La
					// Should also update mstatus to reflect correct mode.
					uint32_t startmstatus = state->mstatus;
					state->mstatus = (( state->mstatus & 0x80) >> 4) | ((state->extraflags&3) << 11) | 0x80;
					state->extraflags = (state->extraflags & ~3) | ((startmstatus >> 11) & 3);
					pc = state->mepc-4;
				}
				else
				{
					switch( csrno )
					{
					case 0: retval = (state->extraflags & 3) ? (11+1) : (8+1); break; // ECALL; 8 = "Environment call from U-mode"; 11 = "Environment call from M-mode"
					case 1:	retval = (3+1); break; // EBREAK 3 = "Breakpoint"
					default: retval = (2+1); break; // Illegal opcode.
					}
				}
			}
			else
				retval = (2+1); 				// Note micrrop 0b100 == undefined.
			break;
		}
		case 0b0101111: // RV32A
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t irmid = ( ir>>27 ) & 0x1f;

			rs1 -= MINIRV32_RAM_IMAGE_OFFSET;

			// We don't implement load/store from UART or CLNT with RV32A here.

			if( rs1 >= MINI_RV32_RAM_SIZE-3 )
			{
				retval = (7+1); //Store/AMO access fault
				rval = rs1 + MINIRV32_RAM_IMAGE_OFFSET;
			}
			else
			{
				rval = *((uint32_t*)(image + rs1));

				// Referenced a little bit of https://github.com/franzflasch/riscv_em/blob/master/src/core/core.c
				int dowrite = 1;
				switch( irmid )
				{
					case 0b00010: dowrite = 0; break; //LR.W
					case 0b00011: rval = 0; break; //SC.W (Lie and always say it's good)
					case 0b00001: break; //AMOSWAP.W
					case 0b00000: rs2 += rval; break; //AMOADD.W
					case 0b00100: rs2 ^= rval; break; //AMOXOR.W
					case 0b01100: rs2 &= rval; break; //AMOAND.W
					case 0b01000: rs2 |= rval; break; //AMOOR.W
					case 0b10000: rs2 = ((int)rs2<(int)rval)?rs2:rval; break; //AMOMIN.W
					case 0b10100: rs2 = ((int)rs2>(int)rval)?rs2:rval; break; //AMOMAX.W
					case 0b11000: rs2 = (rs2<rval)?rs2:rval; break; //AMOMINU.W
					case 0b11100: rs2 = (rs2>rval)?rs2:rval; break; //AMOMAXU.W
					default: retval = (2+1); dowrite = 0; break; //Not supported.
				}
				if( dowrite )
					*((uint32_t*)(image + rs1)) = rs2;
			}
			break;
		}
		default: retval = (2+1); // Fault: Invalid opcode.
	}

	if( retval == 0 )
	{
		if( rdid ) regs[rdid] = rval; // Write back register.

		else if( pc - MINIRV32_RAM_IMAGE_OFFSET >= MINI_RV32_RAM_SIZE ) retval = 1 + 1; //Handle misaligned access
		else if( pc & 3 ) retval = 1 + 0; // Handle access violation on instruction read.
		else if( ( state->mip & (1<<7) ) && ( state->mie & (1<<7) /*mtie*/ ) && ( state->mstatus & 0x8 /*mie*/) )
		{
			retval = 0x80000007;
			pc += 4;
		}
	}

	// Handle traps and interrupts.
	if( retval > 0 )
	{
		if( retval & 0x80000000 ) // If prefixed with 0x100, it's an interrupt, not a trap.
		{
			state->mcause = retval;
			state->mtval = 0;
		}
		else
		{
			state->mcause = retval - 1;
			state->mtval = (retval > 5 && retval <= 8)? rval : pc;
		}
		state->mepc = pc; //XXX TRICKY: The kernel advances mepc automatically.
		//state->mstatus & 8 = MIE, & 0x80 = MPIE
		// On an interrupt, the system moves current MIE into MPIE
		state->mstatus = (( state->mstatus & 0x08) << 4) | ((state->extraflags&3) << 11);
		pc = (state->mtvec - 4);

		// XXX TODO: Do we actually want to check here?
		if( !(retval & 0x80000000) )
			state->extraflags = state->extraflags | 3;
		retval = 0;
	}

	state->pc = pc + 4;
	return retval;
}


#endif


