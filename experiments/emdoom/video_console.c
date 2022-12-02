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

/*

void HandleKey( int keycode, int bDown ){

    int rc;

    switch(rc = keycode )
    {
      case XK_Left:	rc = KEY_LEFTARROW;	break;
      case XK_Right:	rc = KEY_RIGHTARROW;	break;
      case XK_Down:	rc = KEY_DOWNARROW;	break;
      case XK_Up:	rc = KEY_UPARROW;	break;
      case XK_Escape:	rc = KEY_ESCAPE;	break;
      case XK_Return:	rc = KEY_ENTER;		break;
      case XK_Tab:	rc = KEY_TAB;		break;
      case XK_F1:	rc = KEY_F1;		break;
      case XK_F2:	rc = KEY_F2;		break;
      case XK_F3:	rc = KEY_F3;		break;
      case XK_F4:	rc = KEY_F4;		break;
      case XK_F5:	rc = KEY_F5;		break;
      case XK_F6:	rc = KEY_F6;		break;
      case XK_F7:	rc = KEY_F7;		break;
      case XK_F8:	rc = KEY_F8;		break;
      case XK_F9:	rc = KEY_F9;		break;
      case XK_F10:	rc = KEY_F10;		break;
      case XK_F11:	rc = KEY_F11;		break;
      case XK_F12:	rc = KEY_F12;		break;
	
      case XK_BackSpace:
      case XK_Delete:	rc = KEY_BACKSPACE;	break;

      case XK_Pause:	rc = KEY_PAUSE;		break;

      case XK_KP_Equal:
      case XK_equal:	rc = KEY_EQUALS;	break;

      case XK_KP_Subtract:
      case XK_minus:	rc = KEY_MINUS;		break;

      case XK_Shift_L:
      case XK_Shift_R:
	rc = KEY_RSHIFT;
	break;
	
      case XK_Control_L:
      case XK_Control_R:
	rc = KEY_RCTRL;
	break;
	
      case XK_Alt_L:
      case XK_Meta_L:
      case XK_Alt_R:
      case XK_Meta_R:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= XK_space && rc <= XK_asciitilde)
	    rc = rc - XK_space + ' ';
	if (rc >= 'A' && rc <= 'Z')
	    rc = rc - 'A' + 'a';
	break;
    }

    event_t event;
	event.type = bDown?ev_keydown:ev_keyup;
	event.data1 = rc;
	D_PostEvent(&event);
}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;


int mousemask = 0;

void HandleButton( int x, int y, int button, int bDown )
{
	int dx = x - lastmousex;
	int dy = y - lastmousey;

	if( bDown )
	{
		mousemask |= 1<<button;
	}
	else
	{
		mousemask &= ~(1<<button);
	}

    event_t event;
	event.type = ev_mouse;
	event.data1 = mousemask;
	event.data2 = dx*10;
	event.data3 = dy*10;
	D_PostEvent(&event);

	lastmousex = x;
	lastmousey = y;
	
}

void HandleMotion( int x, int y, int mask )
{
	int dx = x - lastmousex;
	int dy = y - lastmousey;

	mousemask = mask;


    event_t event;
	event.type = ev_mouse;
	event.data1 = mousemask;
	event.data2 = dx*10;
	event.data3 = dy*10;
	D_PostEvent(&event);


	lastmousex = x;
	lastmousey = y;

}



//
// I_SetPalette
//

static byte lpalette[256*3];

void I_SetPalette (byte* palette)
{
	memcpy(lpalette, palette, sizeof( lpalette ));
    //UploadNewPalette(X_cmap, palette);
	
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
	CNFGSetup( "Doom", SCREENWIDTH, SCREENHEIGHT );
}

void I_StartTic (void)
{
	CNFGHandleInput();
}
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}
void I_StartFrame()
{
}

void I_ShutdownGraphics(void)
{
	exit(0);
}

void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

	uint32_t bmdata[SCREENWIDTH*SCREENHEIGHT];
	for( i = 0; i < SCREENWIDTH*SCREENHEIGHT; i++ )
	{
		//lpalette
		int col = screens[0][i];
		bmdata[i] = (lpalette[col*3+0]<<16)|(lpalette[col*3+1]<<8)|(lpalette[col*3+2]<<0);
	}

	CNFGUpdateScreenWithBitmap( bmdata,SCREENWIDTH, SCREENHEIGHT );
}

*/







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
    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    /*
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }*/

/*
	uint32_t bmdata[SCREENWIDTH*SCREENHEIGHT];
	for( i = 0; i < SCREENWIDTH*SCREENHEIGHT; i++ )
	{
		//lpalette
		int col = screens[0][i];
		bmdata[i] = (lpalette[col*3+0]<<16)|(lpalette[col*3+1]<<8)|(lpalette[col*3+2]<<0);
	}
	*/
	static int lscreenw = SCREENWIDTH/2;
	static int lscreenh = SCREENHEIGHT/4;
	static int lastcolor1 = -1;
	static int lastcolor2 = -1;
#if 1
	int x, y;

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
		sprintf( ctsline, "\x1b[%d;%dH", y+1, 1 );
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
			if( selcolor1 != lastcolor1 ) { cts[2] = '4'; cts[3] = '0' + selcolor1; HWEMIT( cts ); lastcolor1 = selcolor1; }
			if( selcolor2 != lastcolor2 ) { cts[2] = '3'; cts[3] = '0' + selcolor2; HWEMIT( cts ); lastcolor2 = selcolor2; }
			cts2[0] = '0' + col/4;
			HWEMIT( cts2 );
		}
	}
	#endif
}


