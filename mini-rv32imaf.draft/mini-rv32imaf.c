#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

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
	int retval = 0;

	switch( ir & 0x7f )
	{
		case 0b0110111: // LUI
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			if( rdid ) regs[ rdid ] = ( ir & 0xfffff000 ) ;
			break;
		}
		case 0b0010111: // AUIPC
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			if( rdid ) regs[ rdid ] = pc + ( ir & 0xfffff000 );
			break;
		}
		case 0b1101111: // JAL
		{
			uint32_t rdid = (ir >> 7) & 0x1f;
			int32_t reladdy = (((ir>>21)&0x3ff)<<1) | (((ir>>20)&1)<<11) | (ir&0xff000) | ((ir&0x80000000)>>11);
			if( reladdy & 0x00080000 ) reladdy |= 0xfff00000; // Sign extension.
			if( rdid ) regs[ rdid ] = pc + 4;
			pc = pc + reladdy - 4;
			break;
		}
		case 0b1100111: // JALR
		{
			uint32_t imm = ir >> 20;
			uint32_t rdid = (ir >> 7) & 0x1f;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);

			uint32_t t = pc + 4;
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
			break;
		}
		case 0b0100011: // Store
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t rs2 = regs[(ir >> 20) & 0x1f];
			uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir >> 25 ) >> 20 );
			if( addy & 0x800 ) addy |= 0xfffff000;
			addy += rs1;
			switch( ( ir >> 12 ) & 0x7 )
			{
				//SB, SH, SW
				case 0b000: *((uint8_t*)(image + addy)) = rs2; break;
				case 0b001: *((uint16_t*)(image + addy)) = rs2; break;
				case 0b010: *((uint32_t*)(image + addy)) = rs2; break;
				default: retval = -1;
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
				case 0b011: val = rs1 < imm_se; break;
				case 0b010: val = rs1 < imm; break;
				case 0b100: val = rs1 ^ imm_se; break;
				case 0b110: val = rs1 | imm_se; break;
				case 0b111: val = rs1 & imm_se; break;
				case 0b001: val = rs1 << rs2id; break;
				case 0b101: val = (ir & 0x40000000 ) ? ( ((int32_t)rs1) >> rs2id ) : (rs1 >> rs2id); break;
				default: retval = -1;
			}
			if( rdid )
				regs[rdid] = val;
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

		//RV32F
		case 0b0000111: // FLW
		{
			uint32_t imm = ir >> 20;
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t addy = regs[ (ir >> 15) & 0x1f ] + imm_se;
			uint32_t rdid = (ir >> 7) & 0x1f;
			if( rdid )
				regs[rdid] = *((uint32_t*)(image + addy));
			break;
		}
		case 0b0100111: // FSW
		{
			uint32_t rs1 = regs[(ir >> 15) & 0x1f];
			uint32_t imm = ((ir & 0xf80)>>7) | ((ir & 0xfc000000)>>25);
			int32_t imm_se = imm | (( imm & 0x800 )?0xfffff000:0);
			uint32_t addy = regs[ (ir >> 15) & 0x1f ] + imm_se;
			*((uint32_t*)(image + addy)) = rs1;
			break;
		}

		case 0b1000011: // FMADD.S
		case 0b1000111: // FMSUB.S
		case 0b1001011: // FNMSUB.S
		case 0b1001111: // FNMADD.S
		{
			float rs1 = *((float*)&regs[(ir >> 15) & 0x1f]);
			float rs2 = *((float*)&regs[(ir >> 20) & 0x1f]);
			float rs3 = *((float*)&regs[(ir >> 27) & 0x1f]);
			uint32_t rdid = (ir >> 7) & 0x1f;
			
			float sum;
			if( ir & 0x80 )
				sum = -rs1 * rs2;
			else
				sum = rs1 * rs2;
			if( ir & 0x40 )
				sum -= rs3;
			else
				sum += rs3;
			if( rdid )
				*((float*)&regs[rdid]) = sum;
			break;
		}
		
		case 0b1010011: // All other FP ops.
		{	
			// XXX TODO: Need to test.
			float rs1 = *((float*)&regs[(ir >> 15) & 0x1f]);
			uint32_t rs2id = (ir >> 20) & 0x1f;
			float rs2 = *((float*)&regs[rs2id]);
			uint32_t rdid = (ir >> 7) & 0x1f;
			float sum = 0;
			int idset = 0;
			int is_int_set = 0;
			switch( ir >> 25 )
			{
				case 0b0000000: sum = rs1 + rs2; break; // FADD.S
				case 0b0000100: sum = rs1 - rs2; break; // FSUB.S
				case 0b0001000: sum = rs1 * rs2; break; // FMUL.S
				case 0b0001100: sum = rs1 / rs2; break; // FDIV.S
				case 0b0101100: sum = sqrtf( rs1 ); break; // FSQRT.S
				case 0b0010000:
				{
					int signrs1 = (rs1<0)?-1:1;
					int signrs2 = (rs2<0)?-1:1;
					switch( ( ir & 0x7000 ) >> 12 )
					{
					case 0b000: sum = fabsf( rs1 ) * signrs2; break; // FSGNJ.S
					case 0b001: sum = fabsf( rs1 ) *-signrs2; break; // FSGNJN.S
					case 0b010: sum = fabsf( rs1 ) * signrs2*signrs1; break; // FSGNJX.S
					default: retval = -6;
					break;
					}
					break;
				}
				case 0b0010100: sum = ((!!(ir & 0x1000))^(rs1>rs2))?rs1:rs2; break; // FMIN.S/FMAX.S
				case 0b1100000: // FCVT.S / FCVT.WU.S
				{
					if( ir & 0x100000 )
					{
						sum = (uint32_t)regs[(ir >> 15) & 0x1f];
					}
					else
					{
						sum = (int32_t)regs[(ir >> 15) & 0x1f];
					}
					break;
				}
				case 0b1010000:
				{
					uint32_t iret = 0;
					is_int_set = 1;
					switch( ( ir & 0x7000 ) >> 12 )
					{
						case 0b010: idset = rs1 == rs2; break;
						case 0b001: idset = rs1 < rs2; break;
						case 0b000: idset = rs1 <= rs2; break;
						default: retval = -7; break;
					}
					break;
				}
				case 0b1110000: // FCLASS.S
				{
					is_int_set = 1;
					if( rs1 != rs1 )
						idset = 9;
					else if( isinf( rs1 ) )
						idset = (rs1<0)?0:7;
					else if( isnormal( rs1 ) )
						idset = (rs1<0)?1:6;
					else if( rs1 == -0.0 )
						idset = 3;
					else if( rs1 == 0.0 )
						idset = 4;
					else
						idset = (rs1<0)?2:5;
					break;
				}
				case 0b1101000: // FCVT.S.W/WU
				{
					is_int_set = 1;
					if( ir & 0x100000 )
					{
						idset = (uint32_t)rs1;
					}
					else
					{
						idset = (int32_t)rs1;
					}
					break;
				}
				case 0b1111000: sum = rs1; break; // FMV.W.X
				default: retval = -6;
			}
			if( rdid )
			{
				if( is_int_set )
				{
					regs[rdid] = idset;
				}
				else
				{
					*((float*)&regs[rdid]) = sum;
				}
			}
			break;
		}

		default:
		{
			retval = -1;
		}
	}

	if( retval )
	{
		printf( "%d %08x [%08x]  %08x %08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
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

