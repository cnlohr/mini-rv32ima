#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


uint32_t ram_amt = 128*1024*1024;

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

#define MINIRV32WARN( x... ) printf( x );
#define MINIRV32_DECORATE  static
#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000
#define MINI_RV32_RAM_SIZE ram_amt

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
		fprintf( stderr, "./mini-rv32imaf [parameters]\n\t-m [ram amount]\n\t-f [running image]\n\t-b [dtb file]\n\t-c instruction count\n" );
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
			dtb_ptr = ram_amt - dtblen - sizeof( struct InternalCPUState );
			if( fread( ram_image + dtb_ptr, dtblen - sizeof( struct InternalCPUState ), 1, f ) != 1 )
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
	struct InternalCPUState * core = (struct InternalCPUState *)(ram_image + ram_amt - sizeof( struct InternalCPUState ));
	core->pc = MINIRV32_RAM_IMAGE_OFFSET;
	core->registers[10] = 0x00; //hart ID
	core->registers[11] = dtb_ptr?(dtb_ptr+MINIRV32_RAM_IMAGE_OFFSET):0; //dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core->extraflags |= 3; // Machine-mode.

	// Image is loaded.
	uint64_t rt;
	uint64_t lastTime = GetTimeMicroseconds();
	for( rt = 0; rt < instct || instct < 0; rt++ )
	{
		uint32_t elapsedUs = GetTimeMicroseconds() - lastTime;
		int ret = StepInstruction( core, ram_image, 0, elapsedUs );
		switch( ret )
		{
			case 0: break;
			case 1: usleep(1000); break;
			case 0x7777: goto restart;	//syscon code for restart
			case 0x5555: return 0;		//syscon code for power-off
			default: printf( "Unknown failure\n" ); break;
		} 
		lastTime += elapsedUs;
	}
}

//		printf( "%d %08x [%08x] Z:%08x A1:%08x %08x %08x %08x %08x %08x %08x // %08x %08x %08x %08x %08x %08x %08x %08x//x16: %08x %08x %08x %08x %08x %08x %08x %08x\n", retval, pc, ir,
//			regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
//			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
//			regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23] );

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
