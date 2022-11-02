#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber );

uint64_t ram_amt = 16*1024*1024;

//Tricky: First 32 + 4 words are internal CPU state.
uint8_t * ram_image = 0;

void StepInstruction( uint8_t * image, uint32_t vProcAddress );

struct InternalCPUState
{
	uint32_t registers[32];
	uint32_t pc;
	uint32_t reserved[3];
};

int main( int argc, char ** argv )
{
	int i;
	int show_help;
	const char * image_file_name = 0;
	for( i = 1; i < argc; i++ )
	{
		const char * param = argv + i;
		if( param[0] == '-' )
		{
			switch( param[1] )
			{
			case 'm':
				i++;
				if( i < argc )
					ram_amt = SimpleReadNumber( argv[i], ram_amt );
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
		fprintf( stderr, "./mini-rv32imaf [parameters]\n\t-f [image]" );
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
			fprintf( stderr, "Error: Could not fit RAM image (%d bytes) into %d\n", flen, ram_amt );
			return -6;
		}

		ram_imge = malloc( ram_amt );
		if( fread( ram_imge, ram_amt, 1, f ) != 1 )
		{
			fprintf( stderr, "Error: Could not load image.\n" );
			return -7;
		}
		fclose( f );
	}

	// Image is loaded.
	for(;;)
	{
		StepInstruction( ram_image, 0 );
	}
}

int StepInstruction( uint8_t * image, uint32_t vProcAddress )
{
	struct InternalCPUState * state = (void*)((image) + vProcAddress)
	// state is this processor's "state".

	uint32_t pc = state->pc;

	if( pc & 3 || pc >= ram_amt )
	{
		fprintf( stderr, "Error: CPU PC invalid: %08x\n", state->pc );
		return -1;
	}

	uint32_t ir = ram_image[pc];
	int invalid = 0;

	switch( ir & 0x7f )
	{
		case 0b0110111: // LUI
			state->registers[ ( ir >> 7 ) & 0x1f ] = ir >> 12;
			break;
		case 0b0010111: // AUIPC
			state->registers[ ( ir >> 7 ) & 0x1f ] = pc + ( ir & 0xfffff000 )
			break;
		case 0b1101111: // JAL
			state->registers[ ( ir >> 7 ) & 0x1f ] = pc + 4;
			pc = pc + (((ir >> 23) & 0xff) | ( ( ir >> 22 ) & 1 ) << 8 ) | ( ( ir >> 12 ) & 0x3ff ) << 9 ) | ( ( ir >> 31 ) & 1 ) << 19 )) - 4;
			break;
		case 0b1100111: // JALR
			state->registers[ ( ir >> 7 ) & 0x1f ] = pc + 4;
			pc = state->registers[ (ir >> 16) & 0x1f ] + (ir>>20) - 4;
			break;
		case 0b1100011: // Branch
		{
			uint32_t immm4 = (( ir >> 31 ) | ( ( ( ir >> 7 ) & 1 ) << 1 ) | ( ( ( ir >> 25 ) & 0x3f ) << 2 ) | ( ( ( ir >> 8 ) & 0xf ) << 8) - 4;
			int32_t rs1 = state->registers[(ir >> 15) & 0x1f];
			int32_t rs2 = state->registers[(ir >> 20) & 0x1f];
			switch( ( ir >> 12 ) & 0x7 )
			{
				// BEQ, BNE, BLT, BGE, BLTU, BGEU 
				case 0b000: if( rs1 == rs2 ) pc = immm4; break;
				case 0b001: if( rs1 != rs2 ) pc = immm4; break;
				case 0b100: if( rs1 < rs2 ) pc = immm4; break;
				case 0b101: if( rs1 > rs2 ) pc = immm4; break;
				case 0b110: if( (uint32_t)rs1 < (uint32_t)rs2 ) pc = immm4; break;
				case 0b111: if( (uint32_t)rs1 > (uint32_t)rs2 ) pc = immm4; break;
				default: invalid = 1;
			}
			break;
		}
		case 0b0000011: // Load
		{
			uint32_t rd = state->registers[(ir >> 7) & 0x1f];
			uint32_t rs1id = (ir >> 15) & 0x1f;
			switch( ( ir >> 12 ) & 0x7 )
			{
				//LB, LH, LW, LBU, LHU
				case 0b000: state->registers[rs1id] = *((int8_t*)(image + rd)); break;
				case 0b001: state->registers[rs1id] = *((int16_t*)(image + rd)); break;
				case 0b010: state->registers[rs1id] = *((uint32_t*)(image + rd)); break;
				case 0b100: state->registers[rs1id] = *((uint8_t*)(image + rd)); break;
				case 0b101: state->registers[rs1id] = *((uint16_t*)(image + rd)); break;
				default: invalid = 1;
			}
			break;
		case 0b0100011: // Store
		{
			uint32_t rs1 = state->registers[(ir >> 15) & 0x1f];
			uint32_t rs2 = state->registers[(ir >> 20) & 0x1f];
			uint32_t addy = rs1 + ( ( ir >> 7 ) & 0x1f ) + ( ( ir >> 25 ) << 5 );
			switch( ( ir >> 12 ) & 0x7 )
			{
				//SB, SH, SW
				case 0b000: *((uint8_t*)(image + addy)) = rs2; break;
				case 0b001: *((uint16_t*)(image + addy)) = rs2; break;
				case 0b010: *((uint32_t*)(image + addy)) = rs2; break;
				default: invalid = 1;
			}
		}
		case 0b0100011: // Op-immediate
		{
			// Pick up.
		}

	}
	if( invalid )
	{
		fprintf( stderr, "Error PC: %08x / IR: %08x\n", pc, ir );
		return -1;
	}
	state->pc = pc + 4;
	return 0;
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
	uint64_t ret = strtoll( number &endptr, radix );
	if( endptr == number )
	{
		return defaultNumber;
	}
	else
	{
		return ret;
	}

	
}

