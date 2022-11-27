// Copyright 2022 Charles Lohr, you may use this file or any portions herein under any of the BSD, MIT, or CC0 licenses.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

uint32_t ram_amt = 64*1024*1024;

static uint32_t HandleException( uint32_t ir, uint32_t retval );
static uint32_t HandleControlStore( uint32_t addy, uint32_t val );
static uint32_t HandleControlLoad( uint32_t addy );
static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );

#define MINIRV32WARN( x... ) printf( x );
#define MINIRV32_DECORATE  static
#define MINI_RV32_RAM_SIZE ram_amt
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_POSTEXEC( pc, ir, retval ) { if( retval > 0 ) retval = HandleException( ir, retval ); }
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( image, csrno, value );

#include "mini-rv32ima.h"

static uint64_t SimpleReadNumberUInt( const char * number, uint64_t defaultNumber );
static uint64_t GetTimeMicroseconds();
static void reset_keyboard();

//Tricky: First 32 + 4 words are internal CPU state.
uint8_t * ram_image = 0;


int main( int argc, char ** argv )
{
	atexit(reset_keyboard);
	int i;
	long long instct = -1;
	int show_help = 0;
	int single_step = 0;
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
			case 's':
				single_step = 1;
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
		fprintf( stderr, "./mini-rv32imaf [parameters]\n\t-m [ram amount]\n\t-f [running image]\n\t-b [dtb file]\n\t-c instruction count\n\t-s single step with full processor state\n" );
		return 1;
	}

	ram_image = malloc( ram_amt );

restart:
	{
		FILE * f = fopen( image_file_name, "rb" );
		if( !f || ferror( f ) )
		{
			fprintf( stderr, "Error: \"%s\" not found\n", image_file_name );
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
				fprintf( stderr, "Error: \"%s\" not found\n", dtb_file_name );
				return -5;
			}
			fseek( f, 0, SEEK_END );
			long dtblen = ftell( f );
			fseek( f, 0, SEEK_SET );
			dtb_ptr = ram_amt - dtblen - sizeof( struct MiniRV32IMAState );
			if( fread( ram_image + dtb_ptr, dtblen - sizeof( struct MiniRV32IMAState ), 1, f ) != 1 )
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

	// The core lives at the end of RAM.
	struct MiniRV32IMAState * core = (struct MiniRV32IMAState *)(ram_image + ram_amt - sizeof( struct MiniRV32IMAState ));
	core->pc = MINIRV32_RAM_IMAGE_OFFSET;
	core->regs[10] = 0x00; //hart ID
	core->regs[11] = dtb_ptr?(dtb_ptr+MINIRV32_RAM_IMAGE_OFFSET):0; //dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core->extraflags |= 3; // Machine-mode.

	// Image is loaded.
	uint64_t rt;
	uint64_t lastTime = GetTimeMicroseconds();
	int instrs_per_flip = single_step?1:1024;
	for( rt = 0; rt < instct/instrs_per_flip+1 || instct < 0; rt++ )
	{
		uint32_t elapsedUs = 0;
		elapsedUs = GetTimeMicroseconds() - lastTime;
		lastTime += elapsedUs;

		if( single_step )
		{
			uint32_t pc = core->pc;
			uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
			uint32_t ir = 0;
			printf( "PC: %08x ", pc );
			if( pc_offset < MINIRV32_RAM_IMAGE_OFFSET-3 )
			{
				ir = *((uint32_t*)(&((uint8_t*)ram_image)[pc_offset]));
				printf( "[0x%08x] ", ir ); 
			}
			else
			{
				printf( "[xxxxxxxxxx] " ); 
			}
			uint32_t * regs = core->regs;
			printf( "Z:%08x ra:%08x sp:%08x gp:%08x tp:%08x t0:%08x t1:%08x t2:%08x s0:%08x s1:%08x a0:%08x a1:%08x a2:%08x a3:%08x a4:%08x a5:%08x ",
				regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
				regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15] );
			printf( "a6:%08x a7:%08x s2:%08x s3:%08x s4:%08x s5:%08x s6:%08x s7:%08x s8:%08x s9:%08x s10:%08x s11:%08x t3:%08x t4:%08x t5:%08x t6:%08x\n",
				regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23],
				regs[24], regs[25], regs[26], regs[27], regs[28], regs[29], regs[30], regs[31] );
		}

		int ret = MiniRV32IMAStep( core, ram_image, 0, elapsedUs, instrs_per_flip ); // Execute upto 1024 cycles before breaking out.
		switch( ret )
		{
			case 0: break;
			case 1: usleep(100); break;
			case 0x7777: goto restart;	//syscon code for restart
			case 0x5555: return 0;		//syscon code for power-off
			default: printf( "Unknown failure\n" ); break;
		}
	}
}


static uint64_t SimpleReadNumberUInt( const char * number, uint64_t defaultNumber )
{
	if( !number || !number[0] ) return defaultNumber;
	int radix = 10;
	if( number[0] == '0' )
	{
		char nc = number[1];
		number+=2;
		if( nc == 0 ) return 0;
		else if( nc == 'x' ) radix = 16;
		else if( nc == 'b' ) radix = 2;
		else { number--; radix = 8; }
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

static void reset_keyboard()
{
	struct termios term;
	tcgetattr(0, &term);
	term.c_lflag |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &term);
}

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)
#include <windows.h>
static uint64_t GetTimeMicroseconds()
{
	static LARGE_INTEGER lpf;
	LARGE_INTEGER li;

	if( !lpf.QuadPart )
	{
		QueryPerformanceFrequency( &lpf );
	}

	QueryPerformanceCounter( &li );
	return ((uint64_t)li.QuadPart * 1000000LL) / (uint64_t)lpf.QuadPart;
}
#else
#include <sys/time.h>
static uint64_t GetTimeMicroseconds()
{
	struct timeval tv;
	gettimeofday( &tv, 0 );
	return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}
#endif

static uint32_t HandleException( uint32_t ir, uint32_t code )
{
	// Weird opcode emitted by duktape on exit.
	if( code == 3 )
	{
		// Could handle other opcodes here.
	}
	return code;
}

static uint32_t HandleControlStore( uint32_t addy, uint32_t val )
{
	//Special: UART (8250)
	//If writing a byte, allow it to flow into output.
	if( addy == 0x10000000 )
	{
		printf( "%c", val );
		fflush( stdout );
	}
	return 0;
}


static uint32_t HandleControlLoad( uint32_t addy )
{
	// Emulating a 8250 UART
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	if( addy == 0x10000005 )
		return 0x60 | !!byteswaiting;
	else if( addy == 0x10000000 && byteswaiting )
	{
		char rxchar = 0;
		if( read(fileno(stdin), (char*)&rxchar, 1) > 0 ) // Tricky: getchar can't be used with arrow keys.
			return rxchar;
	}
	return 0;
}

static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value )
{
	if( csrno == 0x136 )
	{
		printf( "%d", value ); fflush( stdout );
	}
	if( csrno == 0x137 )
	{
		printf( "%08x", value ); fflush( stdout );
	}
	else if( csrno == 0x138 )
	{
		//Print "string"
		uint32_t ptrstart = value - MINIRV32_RAM_IMAGE_OFFSET;
		uint32_t ptrend = ptrstart;
		while( ptrend < ram_amt )
		{
			if( image[ptrend] == 0 ) break;
			ptrend++;
		}
		if( ptrend != ptrstart )
		{
			int ret = write( fileno(stdout), image + ptrstart, ptrend - ptrstart );
			ret = ret;
		}
		else
		{
			printf( "DEBUG PASSED INVALID PTR (%08x)\n", value );
		}
	}
}

