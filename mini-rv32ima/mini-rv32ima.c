#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

//#define DEBUG_INSTRUCTIONS
#ifdef DEBUG_INSTRUCTIONS
#define INST_DBG( x... ) printf( x );
#else
#define INST_DBG( x... )
#endif


uint64_t SimpleReadNumberUInt( const char * number, uint64_t defaultNumber );

uint64_t ram_amt = 16*1024*1024;

//Tricky: First 32 + 4 words are internal CPU state.
uint8_t * ram_image = 0;

int StepInstruction( uint8_t * image, uint32_t vProcAddress );
void HandleUART( uint8_t * image );

struct InternalCPUState
{
	uint32_t registers[32];
	uint32_t pc;
	uint32_t reserved[3];
};

int main( int argc, char ** argv )
{
	int i;
	int instct = -1;
	int show_help = 0;
	const char * image_file_name = 0;
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
		fprintf( stderr, "./mini-rv32imaf [parameters]\n\t-f [image]\n\t-c instruction count\n" );
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
			fprintf( stderr, "Error: Could not fit RAM image (%ld bytes) into %ld\n", flen, ram_amt );
			return -6;
		}

		ram_image = malloc( ram_amt );
		if( fread( ram_image, flen, 1, f ) != 1)
		{
			fprintf( stderr, "Error: Could not load image.\n" );
			return -7;
		}
		fclose( f );
	}

	// Image is loaded.
	int rt;
	for( rt = 0; rt < instct || instct < 0; rt++ )
	{
		int ret = StepInstruction( ram_image, 0 );
		if( ret ) break;
		HandleUART( ram_image );
	}
}

int StepInstruction( uint8_t * image, uint32_t vProcAddress )
{
	struct InternalCPUState * state = (void*)((image) + vProcAddress);
	// state is this processor's "state".

	uint32_t pc = state->pc;
	uint32_t * regs = state->registers;

	if( pc & 3 || pc >= ram_amt )
	{
		fprintf( stderr, "Error: CPU PC invalid: %08x\n", state->pc );
		return -1;
	}

	uint32_t ir = *(uint32_t*)(ram_image + pc);
	INST_DBG( "PC: %08x / IR: %08x (OPC: %02x)\n", pc, ir, ir & 0x7f );
	int retval = 0;

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
			int32_t reladdy = (((ir>>21)&0x3ff)<<1) | (((ir>>20)&1)<<11) | (ir&0xff000) | ((ir&0x80000000)>>11);
			if( reladdy & 0x00080000 ) reladdy |= 0xfff00000; // Sign extension.
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
				case 0b101: if( rs1 > rs2 ) pc = immm4; break;
				case 0b110: if( (uint32_t)rs1 < (uint32_t)rs2 ) pc = immm4; break;
				case 0b111: if( (uint32_t)rs1 > (uint32_t)rs2 ) pc = immm4; break;
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
			INST_DBG( "LOADING RSVAL: %08x\n", rsval );
			if( rsval < sizeof(struct InternalCPUState) || rsval >= ram_amt-3 )
			{
				retval = -99;
				printf( "Load OOB Access [%08x]\n", rsval );
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
				if( rdid ) regs[rdid] = loaded;
				INST_DBG( "LOAD [%d] = %08x\n", rdid, regs[rdid]);
			}
			break;
		}
		case 0b0100011: // Store
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir >> 25 ) >> 20 );
			if( addy & 0x800 ) addy |= 0xfffff000;
			addy += rs1;
			if( addy < sizeof(struct InternalCPUState) || addy >= ram_amt-3 )
			{
				retval = -99;
				printf( "Store OOB Access [%08x]\n", addy );
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
				INST_DBG( "STORE [%08x] = %08x\n", addy, rs2 );
			}
			break;
		}
		case 0b0010011: // Op-immediate
		{
			uint32_t imm = ir >> 20;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2id = (ir >> 20) & 0x1f;
			uint32_t rdid = (ir >> 7) & 0x1f;
			uint32_t val = 0;
			// ADDI, SLTI, SLTIU, XORI, ORI, ANDI
			switch( (ir>>12)&7 )
			{
				case 0b000: val = rs1 + imm_se; break;
				case 0b001: val = rs1 << rs2id; break;
				case 0b010: val = rs1 < imm; break;
				case 0b011: val = rs1 < imm_se; break;
				case 0b100: val = rs1 ^ imm_se; break;
				case 0b101: val = (ir & 0x40000000 ) ? ( ((int32_t)rs1) >> rs2id ) : (rs1 >> rs2id); break;
				case 0b110: val = rs1 | imm_se; break;
				case 0b111: val = rs1 & imm_se; break;
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
					switch( (ir>>12)&7 )
					{
						case 0b000: val = rs1 * rs2; break; // MUL
						case 0b001: val = ((int64_t)rs1 * (int64_t)rs2) >> 32; break; // MULH
						case 0b010: val = ((int64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHSU
						case 0b011: val = ((uint64_t)rs1 * (uint64_t)rs2) >> 32; break; // MULHU
						case 0b100: val = (int32_t)rs1 / (int32_t)rs2; break; // DIV
						case 0b101: val = rs1 / rs2; break; // DIVU
						case 0b110: val = (int32_t)rs1 % (int32_t)rs2; break; // REM
						case 0b111: val = rs1 % rs2; break; // REMU
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
			// Fence
			retval = 2;
			break;
		}
		case 0b1110011:
		{
			// ECALL/EBREAK
			retval = 1;
			break;
		}
		case 0b0101111: // RV32A
		{
			retval = -100; //Not yet supported.
/*			switch( ir>>27 )
			{
				case 0b00010:
				default:
				retval = -9;
				//XXX TODO
			}*/
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
		printf( "%d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15] );
	}
	
	if( retval < 0 )
	{
		fprintf( stderr, "Error PC: %08x / IR: %08x\n", pc, ir );
		return -1;
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

void HandleUART( uint8_t * image )
{
	//PROVIDE( uarthead = 0xfb000 );
	//PROVIDE( uarttail = 0xfb004 );
	//PROVIDE( uartbuffer = 0xfb100 );
	if( image[0xfb000] == image[0xfb004] ) return;
	while( image[0xfb000] != image[0xfb004] )
	{
		printf( "%c", image[0xfb100+image[0xfb004]] );
		image[0xfb004]++;
	}
	fflush(stdout);
}

