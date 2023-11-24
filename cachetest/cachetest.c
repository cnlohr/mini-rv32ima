// This test is actually more of a test of a lot of the functionality that is 
// used for vrc-rv32ima.  To make sure the added functionality (like cache and
// custom mulh) will work as expected.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MINI_RV32_RAM_SIZE 0x3FF8000
#define MINIRV32_IMPLEMENTATION

static uint64_t GetTimeMicroseconds();
static void ResetKeyboardInput();
static void CaptureKeyboardInput();
static uint32_t HandleControlStore( uint32_t addy, uint32_t val );
static uint32_t HandleControlLoad( uint32_t addy );
static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );
static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno );
static void MiniSleep();
static int IsKBHit();
static int ReadKBByte();

int fail_on_all_faults = 1;
uint8_t * ram_image = 0;
uint8_t * ram_image_shadow = 0;
struct MiniRV32IMAState * core;

#define MINIRV32WARN( x... ) printf( x );
#define MINIRV32_DECORATE  static
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( image, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( image, csrno );

typedef uint32_t uint4[4];
typedef uint32_t uint;

#define MAXICOUNT 1024

#define uint4assign( a, b ) memcpy( a, b, sizeof( uint32_t ) * 4 )
#define MainSystemAccess( blockno ) (&ram_image[(blockno)*16])
#define precise
#define AS_SIGNED(val) ((int32_t)(val))
#define AS_UNSIGNED(val) ((uint32_t)(val))

// Results, Booting Image.ProfileTest
//                                             vvv this is # of times memory was filled.
// 1024 / 4  > POWEROFF@0x00000000643a3945 // 11877 / 1654514
//  384 / 3  > POWEROFF@0x0000000064268147 // 33481 / 1664185
// 3072 / 3  > POWEROFF@0x00000000643fca65 // 2766 / 1649972
// 1024 / 8  > POWEROFF@0x00000000644d3efd // 3525 / 1651561         
// 1029 / 7  > POWEROFF@0x00000000644a3453 // 3511 / 1651337
// 1023 / 3  > POWEROFF@0x000000006442b3c1 // 18722 / 1660258
//  635 / 5  > POWEROFF@0x00000000643ed80f // 7016 / 1652821
// Adding a max fcnt (Equivelent to # of points able to be output from shader.
//  768 /  635 / 3 /  POWEROFF@0x00000000644affab // 24579 / 1662754
//  768 / 1022 / 2 /  POWEROFF@0x00000000642d0e4b // 78297 / 1690837
//  768 / 969  / 3 /  POWEROFF@0x00000000643ad1ce // 8486 / 1651512
//  768 / 969  / 1 /  POWEROFF@0x0000000020d77858 // 45056097 / 45087974  <<< This is not a valid check, please ignore it.
//  768 / 966  / 2 /  POWEROFF@0x00000000643fbd2d // 49606 / 1681855
//  512 / 966  / 2 /  POWEROFF@0x00000000643d1311 // 62878 / 1690250  << Why is this any different?
//  512 / 512  / 2 /  POWEROFF@0x000000006442f1c8 // 69580 / 1690151  << Interesting. 

///////////////////////////////////////////////////////////////////////////////
// Section from shader.
///////////////////////////////////////////////////////////////////////////////

#include "gpucache.h"

///////////////////////////////////////////////////////////////////////////////
// Done section from shader.
///////////////////////////////////////////////////////////////////////////////

// This will need to be written by the cache user.
void FlushRunlet()
{
	int k;

	int cachefillecout = 0;
	int do_debug_flash_flush = 0;
	if( do_debug_flash_flush ) printf( "[" );
	for( k = 0; k < sizeof( cachesetsaddy ) / sizeof( cachesetsaddy[0] ); k++ )
	{
		int a = cachesetsaddy[k];
		if( a )
		{
			int addybase = (a - 1) << 4;
			int j;
			for( j = 0; j < 4; j++ )
			{
				*((uint32_t*)(&ram_image[addybase])) = cachesetsdata[k][j];
				addybase +=4;
			}
			cachesetsaddy[k] = 0;
			cachefillecout++;
		}

		if( k % (CACHE_N_WAY) == (CACHE_N_WAY) - 1) 
		{
			if( do_debug_flash_flush ) printf( "%d", cachefillecout ); 
			cachefillecout = 0;
		}
	}
	if( do_debug_flash_flush ) printf( "]\n" );
	cache_usage = 0;
}


#include "mini-rv32ima.h"
#include "default64mbdtc.h"

#define ram_amt MINI_RV32_RAM_SIZE

static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image );

int main( int argc, char ** argv )
{
	int do_sleep = 1;
	int fixed_update = 0;
	int dtb_ptr = 0;
	int time_divisor = 1;
	long long instct = -1;

	ram_image = malloc( ram_amt );
	ram_image_shadow = malloc( ram_amt );

	if( !ram_image )
	{
		fprintf( stderr, "Error: could not allocate system image.\n" );
		return -4;
	}

//#define UNITTEST

#ifdef UNITTEST

	int j;
	int rseed = 2;
	int ctct = 0;
	printf( "Block-aligned-test: Expect collisions, but the collisions should be OK\n" );
	srand( rseed );
	memset( ram_image, 0, ram_amt );
	for( j = 0; j < 300000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint v = LoadMemInternalRB( addr*4 );
		if( v )
		{
			//printf( "Collision Test at %d %d %d\n", addr, v, val );
			ctct++;
		}
		StoreMemInternalRB( addr*4, val );
		if( cache_usage >= MAX_FCNT )
		{
			FlushRunlet();
		}
	}
	FlushRunlet();
	srand( rseed );
	for( j = 0; j < 300000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint32_t r = LoadMemInternalRB( addr*4 );
		if( r != val )
		{
			//printf( "%d disagrees %d @ %d\n", val, r, addr*4 );
			ctct--;
		}
	}

	if( ctct )
	{
		fprintf( stderr, "Error: Cache test (def) failed\n" );
		return -9;
	}
	memset( ram_image, 0, ram_amt );

	printf( "Base test OK\n" );
	srand( rseed );
	for( j = 0; j < 2000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint v = LoadMemInternal( addr, 4 );
		if( v )
		{
			printf( "Collision Test at %d %d %d\n", addr, v, val );
			ctct++;
		}
		StoreMemInternal( addr, val, 4 );
		if( cache_usage >= MAX_FCNT )
		{
			FlushRunlet();
		}
	}
	FlushRunlet();
	srand( rseed );
	for( j = 0; j < 2000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint32_t r = LoadMemInternal( addr, 4 );
		if( r != val )
		{
			printf( "val %08x disagrees %08x @ %08x\n", val, r, addr );
			ctct--;
		}
	}
	if( ctct )
	{
		fprintf( stderr, "Error: Cache test (4) failed (%d)\n", ctct );
		return -9;
	}
	printf( "4 test OK\n" );
	memset( ram_image, 0, ram_amt );


	srand( rseed );
	for( j = 0; j < 20000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint v = LoadMemInternal( addr, 4 );
		StoreMemInternal( addr, val, 4 );
		if( cache_usage >= MAX_FCNT)
		{
			FlushRunlet();
		}
		uint v2 = LoadMemInternal( addr, 4 );
		if( val != v2 )
		{
			printf( "TEST FAILED Test at %08x (%08x != %08x) %08x\n", addr, v, v2, val );
			return -10;
		}
	}
	printf( "Cache test (4 B) OK\n" );
	memset( ram_image, 0, ram_amt );

	srand( rseed );
	for( j = 0; j < 3000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint len = (rand() & 3)+1;
		val &= ((uint32_t)(-1))>>(32-len*8);
		uint v = LoadMemInternal( addr, len );
		if( v )
		{
			//printf( "Collision Test at %d %d %d\n", addr, v, val );
			ctct++;
		}
		StoreMemInternal( addr, val, len );
		if( cache_usage >= MAX_FCNT )
		{
			FlushRunlet();
		}
	}
	FlushRunlet();
	srand( rseed );
	for( j = 0; j < 3000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint len = (rand() & 3)+1;
		val &= (((uint32_t)-1)>>(32-len*8));

		uint32_t r = LoadMemInternal( addr, len );
		if( r != val )
		{
			//printf( "%d disagrees %d @ %d\n", val, r, addr );
			ctct--;
		}
	}

	if( ctct )
	{
		fprintf( stderr, "Error: Cache test (r) failed\n" );
		return -9;
	}
	memset( ram_image, 0, ram_amt );
	printf( "Cache Test (R) OK\n" );


	srand( rseed );
	for( j = 0; j < 200000; j++ )
	{
		uint addr = rand() & 2097151;
		uint val = rand();
		uint len = (rand() & 3)+1;
		val &= (((uint32_t)-1)>>(32-len*8));
		uint v = LoadMemInternal( addr, len );
		StoreMemInternal( addr, val, len );
		if( cache_usage  >= MAX_FCNT)
		{
			FlushRunlet();
		}
		uint v2 = LoadMemInternal( addr, 4 );
		v2 &= (((uint32_t)-1)>>(32-len*8));
		if( val != v2 )
		{
			printf( "TEST FAILED Test at %08x (%08x != %08x) %08x\n", addr, v2, val, v );
			return -10;
		}
	}
	printf( "Cache test (R B) OK\n" );
	memset( ram_image, 0, ram_amt );


	exit(0  );
#endif

restart:

	{
		const char * image_file_name = argv[1];
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
	}

	dtb_ptr = ram_amt - sizeof(default64mbdtb) - sizeof( struct MiniRV32IMAState );
	memcpy( ram_image + dtb_ptr, default64mbdtb, sizeof( default64mbdtb ) );


	// Update system ram size in DTB (but if and only if we're using the default DTB)
	// Warning - this will need to be updated if the skeleton DTB is ever modified.
	uint32_t * dtb = (uint32_t*)(ram_image + dtb_ptr);
	if( dtb[0x13c/4] == 0x00c0ff03 )
	{
		uint32_t validram = dtb_ptr;
		dtb[0x13c/4] = (validram>>24) | ((( validram >> 16 ) & 0xff) << 8 ) | (((validram>>8) & 0xff ) << 16 ) | ( ( validram & 0xff) << 24 );
	}



	memcpy( ram_image_shadow, ram_image, ram_amt );

	CaptureKeyboardInput();

	// The core lives at the end of RAM.
	core = (struct MiniRV32IMAState *)(ram_image + ram_amt - sizeof( struct MiniRV32IMAState ));
	core->pc = MINIRV32_RAM_IMAGE_OFFSET;
	core->regs[10] = 0x00; //hart ID
	core->regs[11] = dtb_ptr?(dtb_ptr+MINIRV32_RAM_IMAGE_OFFSET):0; //dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core->extraflags |= 3; // Machine-mode.

	uint64_t rt;
	uint64_t lastTime = (fixed_update)?0:(GetTimeMicroseconds()/time_divisor);
	int instrs_per_flip = MAXICOUNT;
	int total_exits = 0;
	int cache_exits = 0;
	for( rt = 0; rt < instct+1 || instct < 0; rt += instrs_per_flip )
	{
		uint64_t * this_ccount = ((uint64_t*)&core->cyclel);
		uint32_t elapsedUs = 0;
		if( fixed_update )
			elapsedUs = *this_ccount / time_divisor - lastTime;
		else
			elapsedUs = GetTimeMicroseconds()/time_divisor - lastTime;
		lastTime += elapsedUs;

		//if( single_step )
		//	DumpState( core, ram_image);

		int ret = MiniRV32IMAStep( core, ram_image, 0, elapsedUs, instrs_per_flip ); // Execute upto 1024 cycles before breaking out.
		if( cache_usage  >= MAX_FCNT)
		{
			FlushRunlet();
			cache_exits++;
		}
		total_exits++;

		switch( ret )
		{
			case 0: break;
			case 1: if( do_sleep ) MiniSleep(); *this_ccount += instrs_per_flip; break;
			case 3: instct = 0; break;
			case 0x7777: goto restart;	//syscon code for restart
			case 0x5555: printf( "POWEROFF@0x%08x%08x // %d / %d\n", core->cycleh, core->cyclel, cache_exits, total_exits ); return 0; //syscon code for power-off
			default: printf( "Unknown failure\n" ); break;
		}
	}
}



#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

#include <windows.h>
#include <conio.h>

#define strtoll _strtoi64

static void CaptureKeyboardInput()
{
	system(""); // Poorly documented tick: Enable VT100 Windows mode.
}

static void ResetKeyboardInput()
{
}

static void MiniSleep()
{
	Sleep(1);
}

static uint64_t GetTimeMicroseconds()
{
	static LARGE_INTEGER lpf;
	LARGE_INTEGER li;

	if( !lpf.QuadPart )
		QueryPerformanceFrequency( &lpf );

	QueryPerformanceCounter( &li );
	return ((uint64_t)li.QuadPart * 1000000LL) / (uint64_t)lpf.QuadPart;
}


static int IsKBHit()
{
	return _kbhit();
}

static int ReadKBByte()
{
	// This code is kind of tricky, but used to convert windows arrow keys
	// to VT100 arrow keys.
	static int is_escape_sequence = 0;
	int r;
	if( is_escape_sequence == 1 )
	{
		is_escape_sequence++;
		return '[';
	}

	r = _getch();

	if( is_escape_sequence )
	{
		is_escape_sequence = 0;
		switch( r )
		{
			case 'H': return 'A'; // Up
			case 'P': return 'B'; // Down
			case 'K': return 'D'; // Left
			case 'M': return 'C'; // Right
			case 'G': return 'H'; // Home
			case 'O': return 'F'; // End
			default: return r; // Unknown code.
		}
	}
	else
	{
		switch( r )
		{
			case 13: return 10; //cr->lf
			case 224: is_escape_sequence = 1; return 27; // Escape arrow keys
			default: return r;
		}
	}
}

#else

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

static void CtrlC()
{
	DumpState( core, ram_image);
	exit( 0 );
}

// Override keyboard, so we can capture all keyboard input for the VM.
static void CaptureKeyboardInput()
{
	// Hook exit, because we want to re-enable keyboard.
	atexit(ResetKeyboardInput);
	signal(SIGINT, CtrlC);

	struct termios term;
	tcgetattr(0, &term);
	term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
	tcsetattr(0, TCSANOW, &term);
}

static void ResetKeyboardInput()
{
	// Re-enable echo, etc. on keyboard.
	struct termios term;
	tcgetattr(0, &term);
	term.c_lflag |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &term);
}

static void MiniSleep()
{
	usleep(500);
}

static uint64_t GetTimeMicroseconds()
{
	struct timeval tv;
	gettimeofday( &tv, 0 );
	return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}

static int is_eofd;

static int ReadKBByte()
{
	if( is_eofd ) return 0xffffffff;
	char rxchar = 0;
	int rread = read(fileno(stdin), (char*)&rxchar, 1);

	if( rread > 0 ) // Tricky: getchar can't be used with arrow keys.
		return rxchar;
	else
		return -1;
}

static int IsKBHit()
{
	if( is_eofd ) return -1;
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	if( !byteswaiting && write( fileno(stdin), 0, 0 ) != 0 ) { is_eofd = 1; return -1; } // Is end-of-file for 
	return !!byteswaiting;
}


#endif


//////////////////////////////////////////////////////////////////////////
// Rest of functions functionality
//////////////////////////////////////////////////////////////////////////


static uint32_t HandleControlStore( uint32_t addy, uint32_t val )
{
	if( addy == 0x10000000 ) //UART 8250 / 16550 Data Buffer
	{
		printf( "%c", val );
		fflush( stdout );
	}
	return 0;
}


static uint32_t HandleControlLoad( uint32_t addy )
{
	// Emulating a 8250 / 16550 UART
	if( addy == 0x10000005 )
		return 0x60 | IsKBHit();
	else if( addy == 0x10000000 && IsKBHit() )
		return ReadKBByte();
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
		if( ptrstart >= ram_amt )
			printf( "DEBUG PASSED INVALID PTR (%08x)\n", value );
		while( ptrend < ram_amt )
		{
			if( image[ptrend] == 0 ) break;
			ptrend++;
		}
		if( ptrend != ptrstart )
			fwrite( image + ptrstart, ptrend - ptrstart, 1, stdout );
	}
	else if( csrno == 0x139 )
	{
		putchar( value ); fflush( stdout );
	}
}

static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno )
{
	if( csrno == 0x140 )
	{
		if( !IsKBHit() ) return -1;
		return ReadKBByte();
	}
	return 0;
}

static int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber )
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

static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image )
{
	uint32_t pc = core->pc;
	uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
	uint32_t ir = 0;

	printf( "PC: %08x ", pc );
	if( pc_offset >= 0 && pc_offset < ram_amt - 3 )
	{
		ir = *((uint32_t*)(&((uint8_t*)ram_image)[pc_offset]));
		printf( "[0x%08x] ", ir ); 
	}
	else
		printf( "[xxxxxxxxxx] " ); 
	uint32_t * regs = core->regs;
	printf( "Z:%08x ra:%08x sp:%08x gp:%08x tp:%08x t0:%08x t1:%08x t2:%08x s0:%08x s1:%08x a0:%08x a1:%08x a2:%08x a3:%08x a4:%08x a5:%08x ",
		regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
		regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15] );
	printf( "a6:%08x a7:%08x s2:%08x s3:%08x s4:%08x s5:%08x s6:%08x s7:%08x s8:%08x s9:%08x s10:%08x s11:%08x t3:%08x t4:%08x t5:%08x t6:%08x\n",
		regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23],
		regs[24], regs[25], regs[26], regs[27], regs[28], regs[29], regs[30], regs[31] );
}


