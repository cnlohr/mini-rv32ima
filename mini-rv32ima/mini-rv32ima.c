#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <termios.h>

//#define DEBUG_INSTRUCTIONS
#ifdef DEBUG_INSTRUCTIONS
#define INST_DBG( x... ) printf( x );
#else
#define INST_DBG( x... )
#endif

#ifdef SOME_EXTRA_DEBUG
#define INST_INFO( x... ) printf( x );
#else
#define INST_INFO( x... )
#endif

uint64_t SimpleReadNumberUInt( const char * number, uint64_t defaultNumber );

uint32_t ram_amt = 64*1024*1024;
uint32_t ram_image_offset = 0x80000000;

//Tricky: First 32 + 4 words are internal CPU state.
uint8_t * ram_image = 0;

struct InternalCPUState
{
	uint32_t registers[32];
	uint32_t pc;
	uint32_t mscratch;
	uint32_t mtvec;
	uint32_t mie;
	uint32_t mip;
	uint32_t mstatus;
	uint32_t mepc;
	uint32_t mtval;
	uint32_t mcause;
	uint32_t cycleh;
	uint32_t cyclel;
	uint32_t timerh;
	uint32_t timerl;
	uint32_t timermatchh;
	uint32_t timermatchl;
	uint32_t privilege; // Note: only like a few bits are used.  (Machine = 3, User = 0)
	uint8_t uart8250[8]; //@248
	uint8_t * image;
};


int StepInstruction( struct InternalCPUState * state, uint8_t * image, uint32_t vProcAddress );

void reset_keyboard()
{
	struct termios term;
	tcgetattr(0, &term);
	term.c_lflag |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &term);
}


int main( int argc, char ** argv )
{
    atexit(reset_keyboard);

	struct InternalCPUState core = { 0 };
	int i;
	long long instct = -1;
	int show_help = 0;
	int dtb_ptr = 0;
	const char * image_file_name = 0;
	const char * dtb_file_name = 0;
	for( i = 1; i < argc; i++ )
	{
		const char * param = argv[i];
		if( param[0] == '-' )
		{
			switch( param[1] )
			{
			case 'm':
				i++;
				if( i < argc )
					ram_amt = SimpleReadNumberUInt( argv[i], ram_amt );
				break;
			case 'c':
				i++;
				if( i < argc )
					instct = SimpleReadNumberUInt( argv[i], -1 );
				break;
			case 'f':
				i++;
				image_file_name = (i<argc)?argv[i]:0;
				break;
			case 'b':
				i++;
				dtb_file_name = (i<argc)?argv[i]:0;
				break;
			default:
				show_help = 1;
				break;
			}
		}
		else
		{
			show_help = 1;
		}
	}
	if( show_help || image_file_name == 0 )
	{
		fprintf( stderr, "./mini-rv32imaf [parameters]\n\t-f [running image]\n\t-b [dtb file]\n\t-c instruction count\n" );
		return 1;
	}

	{
		FILE * f = fopen( image_file_name, "rb" );
		if( !f || ferror( f ) )
		{
			fprintf( stderr, "Error: Could not open image \"%s\"\n", image_file_name );
			return -5;
		}
		fseek( f, 0, SEEK_END );
		long flen = ftell( f );
		fseek( f, 0, SEEK_SET );
		if( flen > ram_amt )
		{
			fprintf( stderr, "Error: Could not fit RAM image (%ld bytes) into %d\n", flen, ram_amt );
			return -6;
		}

		ram_image = malloc( ram_amt );
		memset( ram_image, 0, ram_amt );
		if( fread( ram_image, flen, 1, f ) != 1)
		{
			fprintf( stderr, "Error: Could not load image.\n" );
			return -7;
		}
		fclose( f );
		
		if( dtb_file_name )
		{
			f = fopen( dtb_file_name, "rb" );
			if( !f || ferror( f ) )
			{
				fprintf( stderr, "Error: Could not open dtb \"%s\"\n", dtb_file_name );
				return -5;
			}
			fseek( f, 0, SEEK_END );
			long dtblen = ftell( f );
			fseek( f, 0, SEEK_SET );
			dtb_ptr = ram_amt - dtblen;
			if( fread( ram_image + dtb_ptr, dtblen, 1, f ) != 1 )
			{
				fprintf( stderr, "Error: Could not open dtb \"%s\"\n", dtb_file_name );
				return -9;
			}
			fclose( f );
		}
	}

	{
		struct termios term;
		tcgetattr(0, &term);
		term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
		tcsetattr(0, TCSANOW, &term);
	}

	core.pc = ram_image_offset;
	core.registers[10] = 0x00; //hart ID
	core.registers[11] = dtb_ptr?(dtb_ptr+0x80000000):0; //dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core.privilege = 3; // Machine-mode.
	core.image = ram_image;

	// Image is loaded.
	long long rt;
	for( rt = 0; rt < instct || instct < 0; rt++ )
	{
		int ret = StepInstruction( &core, ram_image, 0 );
		if( ret == 2 )
		{
			//HandleUART( &core, ram_image );
		}
		else if( ret != 0 )
		{
			break;
		}
	}
}

// https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
// Generally, support for Zicsr
int ReadCSR( struct InternalCPUState * state, int csr )
{
	//printf( "READ: %04x            @ %08x\n", csr, state->pc );
	switch( csr )
	{
	case 0x340: return state->mscratch; break;
	case 0x305: return state->mtvec; break;
	case 0x304: return state->mie; break;
	case 0xC00: return state->cyclel; break;
	case 0x344: return state->mip; break;
	case 0x341: return state->mepc; break;
	case 0x300: return state->mstatus; //mstatus
	case 0x342: return state->mcause; break;
	case 0x343: return state->mtval; break;


	case 0x3B0: return 0; break; //pmpaddr0
	case 0x3a0: return 0; break; //pmpcfg0
	case 0xf11: return 0xff0ff0ff; break; //mvendorid
	case 0xf12: return 0x00000000; break; //marchid
	case 0xf13: return 0x00000000; break; //mimpid
	case 0xf14: return 0x00000000; break; //mhartid
	case 0x301: return 0x40001101; break; //misa (XLEN=32, IMA) TODO: Consider setting X bit.
	default:
		printf( "READ CSR: %08x\n", csr );
		return 0;
	}
}

void WriteCSR( struct InternalCPUState * state, int csr, uint32_t value )
{
	//if( csr == 0x341 )
	//	printf( "WRITE: %04x = %08x (%08x)\n", csr, value, state->pc );
	switch( csr )
	{
	case 0x137: 
	{
		// Special, side-channel printf.
		printf( "SIDE-CHANNEL-DEBUG: %s\n", state->image + value - 0x80000000 );

		uint32_t * regs = state->registers;
		printf( "%08x Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", state->pc,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
		break;
	}
	case 0x138: 
	{
		// Special, side-channel printf.
		printf( "SIDE-CHANNEL-DEBUG: %08x\n", value );

		uint32_t * regs = state->registers;
		printf( "%08x Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", state->pc,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
		break;
	}
	case 0x340: state->mscratch = value; break;
	case 0x305: state->mtvec = value; break;
	case 0x304: state->mie = value; break;
	case 0x344: state->mip = value; break;
	case 0x341: state->mepc = value; break;
	case 0x300: state->mstatus = value; break; //mstatus
	case 0x342: state->mcause = value; break;
	case 0x343: state->mtval = value; break;
	case 0x3a0: break; //pmpcfg0
	case 0x3B0: break; //pmpaddr0
	case 0xf11: break; //mvendorid
	case 0xf12: break; //marchid
	case 0xf13: break; //mimpid
	case 0xf14: break; //mhartid
	case 0x301: break; //misa
	default:
		printf( "WRITE CSR: %08x = %08x\n", csr, value );
	}
}


int StepInstruction( struct InternalCPUState * state, uint8_t * image, uint32_t vProcAddress )
{
	uint32_t * regs = state->registers;
	static int alsolog;

//XXX CNL XXX TODO PICK UP HERE!!!
#if 1
	if( ( state->timerh > state->timermatchh || ( state->timerh == state->timermatchh && state->timerl > state->timermatchl ) ) && ( state->timermatchh || state->timermatchl )  )
	{
		// Fire interrupt.
		// https://stackoverflow.com/a/61916199/2926815
		state->mip |= 1<<7; //MSIP of MIP
	}
	else
	{
		state->mip &= ~(1<<7);
	}

	//state->mstatus & 8 = MIE, & 0x80 = MPIE
	// On an interrupt, the system moves current MIE into MPIE
	if( ( state->mip & state->mie & (1<<7) /*mtie*/ ) && ( state->mstatus & 0x8 /*mie*/) )
	{


		//printf( "TIMER %08x Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", state->pc,
		//	regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
		//	regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
		//	regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );

		// Force machine mode.
		//int tprl = 3;
		//printf( "TIMER: %08x /// %08x %08x // MS: %08x\n", state->mstatus, state->mip, state->mie,state->mstatus );
		//printf( "MSTATUS: %08x\n", state->mstatus );
		state->mstatus = (( state->mstatus & 0x08) << 4) | (state->privilege << 11 );
		state->privilege = 3; // HMM THERE ARE CONDITION WHERE THIS IS NOT TRUE.  XXX CNL XXX  ===>>> ACTUALLY IT IS ALWAYS 3 BUT WE SHOULD PUT OLD STATUS IN???
		//printf( "MSTATUS: %08x\n", state->mstatus );
		state->mepc = state->pc;
		state->mtval = 0;
		state->mcause = 0x80000007; //MSB = "Interrupt 1" 7 = "Machine timer interrupt"
		state->pc = state->mtvec;

		return 0;
#if 0
		printf( "TIMER: %08x Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", state->pc,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
		printf( "TIMER: %08x  => MEPC: %p MTVEC %p;; MS: %08x\n", state->mie, state->mepc, state->mtvec, state->mstatus );
#endif
		//alsolog = 25;
	}
#endif


	uint32_t pc = state->pc;

	//if( pc == 0x80046f00 ) alsolog = 10;

	uint32_t ofs_pc = pc - ram_image_offset;
	if( ofs_pc & 3 || ofs_pc >= ram_amt )
	{
		fprintf( stderr, "Error: CPU PC invalid: %08x\n", state->pc );
		return -1;
	}

	uint32_t ir = *(uint32_t*)(ram_image + ofs_pc);
	INST_DBG( "PC: %08x / IR: %08x (OPC: %02x)\n", pc, ir, ir & 0x7f );
	int retval = 0;

	if( pc == 0x80046f08 )
	{
		printf( "=========================================STACK FAIL\n");
	}

#if 1
	// Print debug info ever random number of cycles.
	if(  (state->cyclel % 9948247 ) ); else
	{
		INST_INFO( "SPART: %d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
	}
#endif
#if 1
	if( alsolog )
	{
		if( alsolog ) alsolog--;
		printf( "SPART: %d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
	}
#endif

	switch( ir & 0x7f )
	{
		case 0b0110111: // LUI
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			if( rdid ) regs[ rdid ] = ( ir & 0xfffff000 ) ;
			INST_DBG( "LUI REG %d = %08x\n", rdid, regs[ rdid ]  );
			break;
		}
		case 0b0010111: // AUIPC
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			if( rdid ) regs[ rdid ] = pc + ( ir & 0xfffff000 );
			INST_DBG( "AUIPC [%d] = %08x\n", rdid, regs[rdid] );
			break;
		}
		case 0b1101111: // JAL
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			//int32_t reladdy = (((ir>>21)&0x3ff)<<1) | (((ir>>20)&1)<<11) | (ir&0xff000) | ((ir&0x80000000)>>11);
			int32_t reladdy = ((ir & 0x80000000)>>11) | ((ir & 0x7fe00000)>>20) | ((ir & 0x00100000)>>9) | ((ir&0x000ff000));
			INST_DBG( "JAL PREADDR: %08x\n", reladdy );
			if( reladdy & 0x00100000 ) reladdy |= 0xffe00000; // Sign extension.
			if( rdid ) regs[ rdid ] = pc + 4;
			INST_DBG( "JAL PC:%08x = %08x + %08x - 4\n",  pc + reladdy - 4, pc, reladdy );
			pc = pc + reladdy - 4;
			break;
		}
		case 0b1100111: // JALR
		{
			uint32_t imm = ir >> 20;
			uint32_t rdid = (ir >> 7) & 0x1f;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);

			uint32_t t = pc + 4;
			INST_DBG( "JAL PC:%08x = %08x, REGNO:%d, %08x\n",  ( (regs[ (ir >> 15) & 0x1f ] + imm_se) & ~1) - 4, ir, (ir >> 15) & 0x1f, imm_se );
			pc = ( (regs[ (ir >> 15) & 0x1f ] + imm_se) & ~1) - 4;
			if( rdid ) regs[ rdid ] = t;
			break;
		}
		case 0b1100011: // Branch
		{
			uint32_t immm4 = ((ir & 0xf00)>>7) | ((ir & 0x7e000000)>>20) | ((ir & 0x80) << 4) | ((ir >> 31)<<12);
			if( immm4 & 0x1000 ) immm4 |= 0xffffe000;
			int32_t rs1 = regs[(ir >> 15) & 0x1f];
			int32_t rs2 = regs[(ir >> 20) & 0x1f];
			immm4 = pc + immm4 - 4;
			switch( ( ir >> 12 ) & 0x7 )
			{
				// BEQ, BNE, BLT, BGE, BLTU, BGEU 
				case 0b000: if( rs1 == rs2 ) pc = immm4; break;
				case 0b001: if( rs1 != rs2 ) pc = immm4; break;
				case 0b100: if( rs1 < rs2 ) pc = immm4; break;
				case 0b101: if( rs1 >= rs2 ) pc = immm4; break; //BGE
				case 0b110: if( (uint32_t)rs1 < (uint32_t)rs2 ) pc = immm4; break;   //BLTU
				case 0b111: if( (uint32_t)rs1 >= (uint32_t)rs2 ) pc = immm4; break;  //BGEU
				default: retval = -1;
			}
			INST_DBG( "BRANCH\n");
			break;
		}
		case 0b0000011: // Load
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t imm = ir >> 20;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t rsval = rs1 + imm_se;
			uint32_t loaded = 0;
			INST_DBG( "LOADING RSVAL: %08x (%08x + %08x)\n", rsval, rs1, imm_se );
			rsval -= ram_image_offset;
			if( rsval >= ram_amt-3 )
			{
				if( rsval >= 0x90000000 && rsval < 0x90000008 ) 
				{
					int byteswaiting;
					ioctl(0, FIONREAD, &byteswaiting);
					rsval = (rsval - 0x90000000);
					state->uart8250[5] |= 0x60;  // Always ready for more!
					state->uart8250[5] = (state->uart8250[5]&0xfe) | (( byteswaiting > 0 )?1:0);
					int rval = state->uart8250[rsval];

					if( rsval == 0 && ( state->uart8250[5] & 1 ) )
					{
						rval = getchar();
					}

					if( rdid ) regs[rdid] = rval;
				}
				else if( rsval >= 0x82000000 && rsval < 0x82010000 )
				{
					uint32_t val = 0;

					// https://chromitem-soc.readthedocs.io/en/latest/clint.html
					if( rsval == 0x8200bffc )
						val = state->timerh;
					else if( rsval == 0x8200bff8 )
						val = state->timerl;
					else
						printf( "****************FAIL***************************** CLNT Access READ [%08x] (%08x + %08x)\n", rsval, rs1, imm_se );
					if( rdid ) regs[rdid] = val;
				}
				else
				{
					retval = -99;
					printf( "Load OOB Access [%08x] (%08x + %08x)\n", rsval, rs1, imm_se );
				}
			}
			else
			{
				switch( ( ir >> 12 ) & 0x7 )
				{
					//LB, LH, LW, LBU, LHU
					case 0b000: loaded = *((int8_t*)(image + rsval)); break;
					case 0b001: loaded = *((int16_t*)(image + rsval)); break;
					case 0b010: loaded = *((uint32_t*)(image + rsval)); break;
					case 0b100: loaded = *((uint8_t*)(image + rsval)); break;
					case 0b101: loaded = *((uint16_t*)(image + rsval)); break;
					default: retval = -1;
				}
				if( rsval == 0x00293E5C )
				{
					//printf( ">>>>>>>>>>LOADING 80293E5C = %08x, PC: %08x\n", loaded, pc );
				}
				if( rdid ) regs[rdid] = loaded;
				INST_DBG( "LOAD [%d, %08x] = %08x  [%x]\n", rdid,rsval, regs[rdid], ( ir >> 12 ) & 0x7);
			}
			break;
		}
		case 0b0100011: // Store
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir & 0xfe000000 ) >> 20 );
			INST_DBG( "STORE ADDY: %08x + %08x; ", addy, rs1 );
			if( addy & 0x800 ) addy |= 0xfffff000;
			INST_DBG( "%08x\n", addy );
			addy += rs1 - ram_image_offset;

			if( addy >= ram_amt-3 )
			{
				if( addy >= 0x90000000 && addy < 0x90000008 ) 
				{
					//Special: UART.
					if( addy == 0x90000000 )
					{
						printf( "%c", rs2 );
						fflush( stdout );
					}
					else
					{
						//printf( "************************** Write UART: %08x -> %08x\n", addy, rs2 );
					}
					addy = (addy - 0x90000000) - (intptr_t)image + (intptr_t)state->uart8250;
				}
				else if( addy >= 0x82000000 && addy < 0x82010000 )
				{

					if( addy == 0x82004004 )
						state->timermatchh = rs2;
					else if( addy == 0x82004000 )
						state->timermatchl = rs2;
					else
						printf( "CLNT Access WRITE [%08x] = %08x\n", addy, rs2 );
				}
				else
				{
					retval = -99;
					printf( "Store OOB Access [%08x]\n", addy );
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
					default: retval = -1;
				}
				INST_DBG( "STORE [%08x] = %08x [%x]\n", addy, rs2, ( ir >> 12 ) & 0x7 );
				if( addy == 0x00293E5C )
				{
					//printf( ">>>>>>>>>>saving 80293E5C = %08x, PC = %08x\n", rs2, pc);
				}
			}
			break;
		}
		case 0b0010011: // Op-immediate
		{
			uint32_t imm = ir >> 20;
			imm = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2id = (ir >> 20) & 0x1f;
			uint32_t rdid = (ir >> 7) & 0x1f;
			uint32_t val = 0;
			// ADDI, SLTI, SLTIU, XORI, ORI, ANDI
			switch( (ir>>12)&7 )
			{
				case 0b000: val = rs1 + imm; break;
				case 0b001: val = rs1 << rs2id; break;
				case 0b010: val = (int)rs1 < (int)imm; break;  //signed (SLTI)
				case 0b011: val = rs1 < imm; break; //unsigned (SLTIU)
				case 0b100: val = rs1 ^ imm; break;
				case 0b101: val = (ir & 0x40000000 ) ? ( ((int32_t)rs1) >> rs2id ) : (rs1 >> rs2id); break;
				case 0b110: val = rs1 | imm; break;
				case 0b111: val = rs1 & imm; break;
				default: retval = -1;
			}
			if( rdid )
				regs[rdid] = val;
			INST_DBG( "OP-IMMEDIATE:%d = %08x\n", rdid, regs[rdid] );
			break;
		}
		case 0b0110011: // Op
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t rdid = (ir >> 7) & 0x1f;
			uint32_t val = 0;
			if( rdid )
			{
				if( ir & 0x02000000 )
				{
					// RV32M
					// XXX TODO: Check MULH/MULHSU/MULHU

					{
						switch( (ir>>12)&7 )
						{
							case 0b000: val = rs1 * rs2; break; // MUL
							case 0b001: val = ((int64_t)rs1 * (int64_t)rs2) >> 32; break; // MULH
							case 0b010: val = ((int64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHSU
							case 0b011: val = ((uint64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHU
							case 0b100: if( rs2 == 0 ) val = -1; else val = (int32_t)rs1 / (int32_t)rs2; break; // DIV
							case 0b101: if( rs2 == 0 ) val = 0xffffffff; else val = rs1 / rs2; break; // DIVU
							case 0b110: if( rs2 == 0 ) val = rs1; else val = (int32_t)rs1 % (int32_t)rs2; break; // REM
							case 0b111: if( rs2 == 0 ) val = rs1; else val = rs1 % rs2; break; // REMU
						}
					}
				}
				else
				{
					switch( (ir>>12)&7 )
					{
						case 0b000: val = (ir & 0x40000000 ) ? ( rs1 - rs2 ) : ( rs1 + rs2 ); break;
						case 0b001: val = rs1 << rs2; break;
						case 0b010: val = (int32_t)rs1 < (int32_t)rs2; break;
						case 0b011: val = rs1 < rs2; break;
						case 0b100: val = rs1 ^ rs2; break;
						case 0b101: val = (ir & 0x40000000 ) ? ( ((int32_t)rs1) >> rs2 ) : ( rs1 >> rs2 ); break;
						case 0b110: val = rs1 | rs2; break;
						case 0b111: val = rs1 & rs2; break;
					}
				}
				regs[rdid] = val;
				INST_DBG( "OP:%d = %08x\n", rdid, regs[rdid] );
			}
			break;
		}
		case 0b0001111:
		{
			int fencetype = (ir >> 12) & 0b111;
			// Fence
			//printf( "FENCE %d\n", fencetype );
			//retval = 2;
			break;
		}
		case 0b1110011:  // Zifencei+Zicsr
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			int rs1imm = (ir >> 15) & 0x1f;
			uint32_t rs1 = regs[rs1imm];
			uint32_t csrno = ir >> 20;

			int microop = ( ir >> 12 ) & 0b111;
			uint32_t writeval = rs1;
			int do_write = !!(microop & 3);
			uint32_t readval = 0;

			if( (microop & 3) )
			{
				readval = ReadCSR( state, csrno );
			}

			switch( microop )
			{
			case 0b000: //ECALL/EBREAK/WFI
				if( csrno == 0x105 )
					;// WFI, Ignore.
				else if( ( ( csrno & 0xff ) == 0x02 ) )
				{
#if 0
					printf( "MRET<< %p %p %p %p   MRET TYPE:[%d]\n", state->mie, state->mip, state->mstatus, state->mepc, ir >> 28 );

					//URET, MRET, SRET, HRET
					printf( "MRET!!! %d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
						regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
						regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
						regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
#endif
					// MIE = MPIE -> Trap Return 

	//				printf( "MRSTAT: %08x\n", state->mstatus );
					uint32_t startmstatus = state->mstatus;
					//state->privilege = (ir & 0x30000000)>>28;  // I thought you could get this from mret/sret, but I guess not?
					state->mstatus = (( state->mstatus & 0x80) >> 4) | (0 << 11) | 0x80;
//					printf( "MRSTAT: %08x  %08x  [[%08x]]\n", state->mstatus, startmstatus, ir );
					if( ir != 0x30200073 ) printf( "********************************************************\n" );
				//	alsolog = 10;

					state->privilege = (startmstatus >> 11) & 3;

					pc = state->mepc-4;
	//				printf( "<<MRET>> %08x %08x %08x MEPC = %08x THIS PC %08x\n", state->mie, state->mip, state->mstatus, state->mepc, pc);

					//https://raw.githubusercontent.com/riscv/virtual-memory/main/specs/663-Svpbmt.pdf
					//Table 7.6. MRET then in mstatus/mstatush sets MPV=0, MPP=0, MIE=MPIE, and MPIE=1. La
					// Should also update mstatus to reflect correct mode.
					rdid = 0; do_write= 0;
					
					//alsolog = 15;
				}
				else
				{
					if( (ir >> 24) == 0xff )
					{
						printf( "Custom opcode for force exit\n" );
						exit(1);
					}

					if( csrno == 0 )
					{
						printf( "ECALL ECALL ECALL @ %08x\n", pc );//retval = 1;						
						state->mcause = 8; // 8 = "Environment call from U-mode"; 11 = "Environment call from M-mode"
					}
					else
					{
						printf( "EBREAK EBREAK EBREAK @ %08x\n", pc );//retval = 1;
						state->mcause = 3;
					}
					rdid = 0; do_write= 0;

					//printf( "%d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
					//	regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
					//	regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
					//	regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );

					state->mepc = pc; //XXX TRICKY: Looks like the kernel advances mepc
					state->mtval = pc;

//					alsolog = 5;

//					printf( "TRAPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP %08x %08x -> %08x  MEPC=%08x\n", pc, pc, state->mtvec, state->mepc );

//					printf( "EBRK: %08x\n", state->mstatus );
					state->mstatus = (( state->mstatus & 0x08) << 4) | (0 << 11);
					printf( "EBRK2: %08x  [[%08x]]\n", state->mstatus, ir );
					state->privilege = 3;
					pc = (state->mtvec - 4);
				}
				break;
			case 0b001: writeval = rs1; break;  				//CSRRW
			case 0b010: writeval = readval | rs1; break;		//CSRRS
			case 0b011: writeval = readval & ~rs1; break;		//CSRRC
			case 0b100: retval = -98;  break; // Unused
			case 0b101: writeval = rs1imm; break;				//CSRRWI
			case 0b110: writeval = readval | rs1imm; break;		//CSRRSI
			case 0b111: writeval = readval & ~rs1imm; break;	//CSRRCI
			}

			//printf( "Zifencei+Zicsr %08x [%08x] ==> %d; %04x (Read: %08x; Write: %08x / %08x %08x)\n", pc, ir, microop, csrno, readval, writeval, rs1, rs1imm ); 

			if( do_write) WriteCSR( state, csrno, writeval );
			if( rdid ) regs[rdid] = readval;
			break;
		}
		case 0b0101111: // RV32A
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t readval = 0;
			uint32_t irmid = ( ir>>27 ) & 0x1f;

			rs1 -= ram_image_offset;

			if( rs1 >= 0x90000000 && rs1 < 0x90000008 ) 
			{
				//Special: UART.
				rs1 = (rs1 - 0x90000000) - (intptr_t)image + (intptr_t)state->uart8250;
				printf( "************************** RV32A UART: %08x -> %08x\n", rs1, rs2 );
			}
			else if( rs1 >= ram_amt-3 )
			{
				retval = -99;
				printf( "Store OOB Access [%08x]\n", rs2 );
			}
			else
			{
				if( irmid != 0b00011 )
				{
					readval = *((uint32_t*)(image + rs1));
				}
				INST_DBG( "RV32A: %d: %08x ==> %08x\n", irmid, rs1, readval );

				// Referenced a little bit of https://github.com/franzflasch/riscv_em/blob/master/src/core/core.c
				int dowrite = 1;
				switch( irmid )
				{
					case 0b00010: dowrite = 0; break; //LR.W
					case 0b00011: readval = 0;
					
				if( rs1 == 0x00293E5C )
				{
					//printf( ">>>>>>>>>>RV32A 80293E5C = %08x  PC: %08x\n", rs2, pc );
				}

					break; //SC.W (Lie and always say it's good)
					case 0b00001: break; //AMOSWAP.W
					case 0b00000: rs2 += readval; break; //AMOADD.W
					case 0b00100: rs2 ^= readval; break; //AMOXOR.W
					case 0b01100: rs2 &= readval; break; //AMOAND.W
					case 0b01000: rs2 |= readval; break; //AMOOR.W
					case 0b10000: rs2 = ((int)rs2<(int)readval)?rs2:readval; break; //AMOMIN.W
					case 0b10100: rs2 = ((int)rs2>(int)readval)?rs2:readval; break; //AMOMAX.W
					case 0b11000: rs2 = (rs2<readval)?rs2:readval; break; //AMOMINU.W
					case 0b11100: rs2 = (rs2>readval)?rs2:readval; break; //AMOMAXU.W
					default: retval = -100; dowrite = 0; rdid = 0; break; //Not supported.
				}
				if( dowrite )
					*((uint32_t*)(image + rs1)) = rs2;
				if( rdid )
					regs[rdid] = readval;
			}

			break;
		}

		default:
		{
			retval = -1;
		}
	}



#ifndef DEBUG_INSTRUCTIONS
	if( retval )
#endif
	{
		printf( "%d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );
	}

	// Increment both wall-clock and instruction count time.
	++state->cyclel;
	if( state->cyclel == 0 ) state->cycleh++;

	if( ( state->cyclel % 1 ) == 0 )
	{
		state->timerl++;
		if( state->timerl == 0 ) state->timerh++;
	}

	if( retval < 0 )
	{
		fprintf( stderr, "Error PC: %08x / IR: %08x\n", pc, ir );
		return -1;
	}

	if( pc < 10000 )
	{
		INST_INFO( "PC WILL BE INVALID was %08x / %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", state->pc, pc, ir,
						regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
						regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
						regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );

	}

	state->pc = pc + 4;
	return retval;
}

uint64_t SimpleReadNumberUInt( const char * number, uint64_t defaultNumber )
{
	if( !number || !number[0] ) return defaultNumber;
	int radix = 10;
	if( number[0] == '0' )
	{
		number++;
		char nc = number[0];
		if( nc == 0 ) return 0;
		else if( nc == 'x' ) radix = 16;
		else if( nc == 'b' ) radix = 2;
		else radix = 8;
	}
	char * endptr;
	uint64_t ret = strtoll( number, &endptr, radix );
	if( endptr == number )
	{
		return defaultNumber;
	}
	else
	{
		return ret;
	}	
}

#if 0

void HandleUART( struct InternalCPUState * state, uint8_t * image )
{
	//PROVIDE( uarthead = 0xfb000 );
	//PROVIDE( uarttail = 0xfb004 );
	//PROVIDE( uartbuffer = 0xfb100 );
/*
	if( image[0xfb000] == image[0xfb004] ) return;
	while( image[0xfb000] != image[0xfb004] )
	{
		printf( "%c", image[0xfb100+image[0xfb004]] );
		image[0xfb004]++;
	}
	fflush(stdout);
*/
	if( !(state->uart8250[5] & 0x20) )
	{
		printf( "[%08x]", state->uart8250[0] );
		fflush(stdout);
	}
	state->uart8250[5] |= 0x20;
}

#endif

