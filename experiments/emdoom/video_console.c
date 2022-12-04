//Stubbed Video.c, for terminals

#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include "embeddeddoom/src/doomstat.h"
#include "embeddeddoom/src/i_system.h"
#include "embeddeddoom/src/v_video.h"
#include "embeddeddoom/src/m_argv.h"
#include "embeddeddoom/src/d_main.h"

#include "embeddeddoom/src/doomdef.h"


#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#define POINTER_WARP_COUNTDOWN	1

//
// I_SetPalette
//

static byte lpalette[256*3];

void I_SetPalette (byte* palette)
{
	memcpy(lpalette, palette, sizeof( lpalette ));
    //UploadNewPalette(X_cmap, palette);
	
}

static int is_eofd;

static void CtrlC()
{
	exit( 0 );
}

static void ResetKeyboardInput();
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

static int ReadKBByte()
{
	if( is_eofd ) return 0xffffffff;
	char rxchar = 0;
	int rread = read(fileno(stdin), (char*)&rxchar, 1);
	if( rread > 0 ) // Tricky: getchar can't be used with arrow keys.
		return rxchar;
	else
		return 0xffffffff;
}

static int IsKBHit()
{
	if( is_eofd ) return 0;
	if( write( fileno(stdin), 0, 0 ) != 0 ) { is_eofd = 1; return 1; } // Is end-of-file.
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	return !!byteswaiting;
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

void I_InitGraphics(void)
{

	CaptureKeyboardInput();
	//CNFGSetup( "Doom", SCREENWIDTH, SCREENHEIGHT );
}

uint8_t downmap[256];

void I_StartTic (void)
{
	    event_t event;
	//CNFGHandleInput();
	
	if( IsKBHit() )
	{
		event.type = ev_keydown;
		int hit =  ReadKBByte();
		switch( hit )
		{
		case 10: hit = KEY_ENTER; break;
		case 'A': case 'a': hit = KEY_LEFTARROW; break;
		case 'S': case 's': hit = KEY_DOWNARROW; break;
		case 'D': case 'd': hit = KEY_RIGHTARROW; break;
		case 'W': case 'w': hit = KEY_UPARROW; break;
		case '/': hit = KEY_RCTRL; break;
		}
		event.data1 = hit;
		D_PostEvent(&event);
		downmap[(uint8_t)event.data1] = 10;
		printf( "DOWN %d\n", event.data1 );
	}
	
	int i;
	for( i = 0; i < 256; i++ )
	{
		if( downmap[i] )
		{
			if( --downmap[i] == 0 )
			{
				event.type = ev_keyup;
				event.data1 = i;
				printf( "UP %d\n", i );
				D_PostEvent(&event);
			}
		}
	}
}

void I_ReadScreen (byte* scr)
{
    memcpy( scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

void I_StartFrame()
{
}

void I_ShutdownGraphics(void)
{
	exit(0);
}

void HWEMIT( const char * s )
{
#ifdef IS_ON_DESKTOP_NOT_RV_EMULATOR
	printf( "%s", s );
#else
	char * uart = (char*)0x10000000;
	char c;
	while( c = *(s++) )
		*uart = c;
#endif
}

void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;

	const int lscreenw = SCREENWIDTH/2;
	const int lscreenh = SCREENHEIGHT/4;
	static int lastcolor1 = -1;
	static int lastcolor2 = -1;

	int x, y;

	// Set cursor to top corner of screen.
	char cts[6];
	cts[0] = 0x1b;
	cts[1] = '[';
	cts[4] = 'm';
	cts[5] = 0;
	char cts2[2] = { 0, 0 };

	char ctsline[128];

	for( y = 0; y < lscreenh; y++ )
	{
		int ly = y * SCREENHEIGHT / lscreenh;
		sprintf( ctsline, "\x1b[%d;%dH", y+1, 1 ); // Set cursor to beginning of specific line.
		HWEMIT( ctsline );
		for( x = 0; x < lscreenw; x++ )
		{
			int lx = x * SCREENWIDTH / lscreenw;
			int col = screens[0][ lx+ly*SCREENWIDTH];
			int r = lpalette[col*3+0]+32;
			int g = lpalette[col*3+1]+32;
			int b = lpalette[col*3+2]+32;
			int selcolor1 = (!!(r&128)) | (!!(g&128))*2 | (!!(b&128))*4;
			int selcolor2 = (!!(r&64)) | (!!(g&64))*2 | (!!(b&64))*4;
			if( selcolor1 != lastcolor1 )
			{
				cts[2] = '4'; cts[3] = '0' + selcolor1;
				HWEMIT( cts );
				lastcolor1 = selcolor1;
			}
			if( selcolor2 != lastcolor2 )
			{
				cts[2] = '3'; cts[3] = '0' + selcolor2;
				HWEMIT( cts );
				lastcolor2 = selcolor2;
			}
			cts2[0] = '0' + col/4;
			HWEMIT( cts2 );
		}
	}
}


