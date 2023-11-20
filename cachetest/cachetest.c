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
static uint32_t HandleException( uint32_t ir, uint32_t retval );
static uint32_t HandleControlStore( uint32_t addy, uint32_t val );
static uint32_t HandleControlLoad( uint32_t addy );
static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );
static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno );
static void MiniSleep();
static int IsKBHit();
static int ReadKBByte();

int fail_on_all_faults = 1;
uint8_t * ram_image = 0;
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
int icount;
#define MAXICOUNT 1024

#define uint4assign( a, b ) memcpy( a, b, sizeof( uint32_t ) * 4 )
#define MainSystemAccess( blockno ) (&ram_image[(blockno)*16])
#define precise
#define AS_SIGNED(val) ((int32_t)(val))
#define AS_UNSIGNED(val) ((uint32_t)(val))

///////////////////////////////////////////////////////////////////////////////
// Section from shader.
///////////////////////////////////////////////////////////////////////////////
			static uint4 cachesetsdata[1024];
			static uint  cachesetsaddy[1024];
			static uint  storeblockcount;
			static uint  need_to_flush_runlet;

			// Always aligned-to-4-bytes.
			uint LoadMemInternalRB( uint ptr )
			{
				int i;
				uint blockno = ptr / 16;
				uint hash = blockno & 0x7f;
				uint4 block;
				uint ct = 0;
				for( i = hash; i += 128; i<1024 )
				{
					ct = cachesetsaddy[i];
					if( ct == 0 ) break;
					if( ct == ptr )
					{
						// Found block.
						uint4assign( block, cachesetsdata[i] );
					}
				}
				if( ct == 0 )
				{
					// else, no block found. Read data.
					uint4assign( block, MainSystemAccess( blockno ) );
				}
				return block[(ptr&0xf)>>2];
			}


			// todo: review all this.
			void StoreMemInternalRB( uint ptr, uint val )
			{
				int i;
				uint blockno = ptr / 16;
				// ptr will be aligned.
				// perform a 4-byte store.
				uint hash = blockno & 0x7f;
				uint4 block;
				uint ct = 0;
				// Cache lines are 8-deep, by 16 bytes, with 128 possible cache addresses.
				for( i = hash; i += 128; i<1024 )
				{
					ct = cachesetsaddy[i];
					if( ct == 0 ) break;
					if( ct == ptr )
					{
						// Found block.
						cachesetsdata[i][(ptr&0xf)>>2] = val;
						return;
					}
				}
				// NOTE: It should be impossible for i to ever be or exceed 1024.
				if( i >= (1024-128) )
				{
					// We have filled a cache line.  We must cleanup without any other stores.
					need_to_flush_runlet = 1;
				}
				cachesetsaddy[i] = blockno;
				uint4assign( block, MainSystemAccess( blockno ) );
				block[(ptr&0xf)>>2] = val;
				uint4assign( cachesetsdata[i], block );
				storeblockcount++;
				// Make sure there's enough room to flush processor state (16 writes)
				if( storeblockcount >= 112 ) need_to_flush_runlet = 1;
			}

			// NOTE: len does NOT control upper bits.
			uint LoadMemInternal( uint ptr, uint len )
			{
				uint remo = ptr & 3;
				if( remo )
				{
					if( len > 4 - remo )
					{
						// Must be split into two reads.
						uint ret0 = LoadMemInternalRB( ptr & (~3) );
						uint ret1 = LoadMemInternalRB( (ptr & (~3)) + 4 );
						return (ret0 >> (remo*8)) | (ret1<<((4-remo)*8)); // XXX TODO:TESTME!!!
					}
					else
					{
						// Can just be one.
						uint ret = LoadMemInternalRB( ptr & (~3) );
						return ret >> (remo*8);
					}
				}
				return LoadMemInternalRB( ptr );
			}
			
			void StoreMemInternal( uint ptr, uint val, uint len )
			{
				uint remo = ptr & 3;
				if( remo )
				{
					if( len > 4 - remo )
					{
						// Must be split into two writes.
						uint ret0 = LoadMemInternalRB( ptr & (~3) );
						uint ret1 = LoadMemInternalRB( (ptr & (~3)) + 4 );
						uint loaded = (ret0 >> (remo*8)) | (ret1<<((4-remo)*8));
						uint mask = (1<<(len*8))-1;
						loaded = (loaded & (~mask)) | ( val & mask );
						// XXX TODO
					}
					else
					{
						// Can just be one call.
						uint ret = LoadMemInternalRB( ptr & (~3) );
						return ret >> (remo*8);
						// XXX TODO
					}
				}
				if( len != 4 )
				{
					uint lv = -( ptr );
					// XXX TODO
				}
				else
				{
					StoreMemInternalRB( ptr, val );
				}
			}

			#define MINIRV32_POSTEXEC( a, b, c ) ;

			#define MINIRV32_CUSTOM_MEMORY_BUS
			uint MINIRV32_LOAD4( uint ofs ) { return LoadMemInternal( ofs, 4 ); }
			void MINIRV32_STORE4( uint ofs, uint val ) { StoreMemInternal( ofs, val, 4 ); if( need_to_flush_runlet ) icount = MAXICOUNT; }
			uint MINIRV32_LOAD2( uint ofs ) { uint tword = LoadMemInternal( ofs, 2 ) & 0xffff; return tword; }
			uint MINIRV32_LOAD1( uint ofs ) { uint tword = LoadMemInternal( ofs, 1 ) & 0xff; return tword; }
			int MINIRV32_LOAD2_SIGNED( uint ofs ) { uint tword = LoadMemInternal( ofs, 2 ) & 0xffff; if( tword & 0x8000 ) tword |= 0xffff; return tword; }
			int MINIRV32_LOAD1_SIGNED( uint ofs ) { uint tword = LoadMemInternal( ofs, 1 ) & 0xff;   if( tword & 0x80 ) tword |= 0xff; return tword; }
			void MINIRV32_STORE2( uint ofs, uint val ) { StoreMemInternal( ofs, val, 2 ); if( need_to_flush_runlet ) icount = MAXICOUNT; }
			void MINIRV32_STORE1( uint ofs, uint val ) { StoreMemInternal( ofs, val, 1 ); if( need_to_flush_runlet ) icount = MAXICOUNT; }

			// From pi_maker's VRC RVC Linux
			// https://github.com/PiMaker/rvc/blob/eb6e3447b2b54a07a0f90bb7c33612aeaf90e423/_Nix/rvc/src/emu.h#L255-L276
			#define CUSTOM_MULH \
				case 1: \
				{ \
				    /* FIXME: mulh-family instructions have to use double precision floating points internally atm... */ \
					/* umul/imul (https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/umul--sm4---asm-)       */ \
					/* do exist, but appear to be unusable?                                                           */ \
					precise double op1 = AS_SIGNED(rs1); \
					precise double op2 = AS_SIGNED(rs2); \
					rval = (uint)((op1 * op2) / 4294967296.0l); /* '/ 4294967296' == '>> 32' */ \
					break; \
				} \
				case 2: \
				{ \
					/* is the signed/unsigned stuff even correct? who knows... */ \
					precise double op1 = AS_SIGNED(rs1); \
					precise double op2 = AS_UNSIGNED(rs2); \
					rval = (uint)((op1 * op2) / 4294967296.0l); /* '/ 4294967296' == '>> 32' */ \
					break; \
				} \
				case 3: \
				{ \
					precise double op1 = AS_UNSIGNED(rs1); \
					precise double op2 = AS_UNSIGNED(rs2); \
					rval = (uint)((op1 * op2) / 4294967296.0l); /* '/ 4294967296' == '>> 32' */ \
					break; \
				}
	



///////////////////////////////////////////////////////////////////////////////
// Done section from shader.
///////////////////////////////////////////////////////////////////////////////



#include "mini-rv32ima.h"
#include "sixtyfourmb.dtb.h"

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

restart:
	if( !ram_image )
	{
		fprintf( stderr, "Error: could not allocate system image.\n" );
		return -4;
	}

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
		switch( ret )
		{
			case 0: break;
			case 1: if( do_sleep ) MiniSleep(); *this_ccount += instrs_per_flip; break;
			case 3: instct = 0; break;
			case 0x7777: goto restart;	//syscon code for restart
			case 0x5555: printf( "POWEROFF@0x%08x%08x\n", core->cycleh, core->cyclel ); return 0; //syscon code for power-off
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


