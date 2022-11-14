#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

extern unsigned int videodata[320*240];
extern unsigned char uartbuffer[256];
extern unsigned char uarthead;
extern unsigned char uarttail;

void _Unwind_Resume() { }
void __gcc_personality_v0() { }

void putuart( char c )
{
	if( ((unsigned char)(uarthead+1)) == uarttail ) return;
	uartbuffer[uarthead] = c;
	uarthead++;
}

void lprint( const char * s )
{
	char c;
	while( c = *(s++) ) putuart( c );
}

void PlotPixel( int x, int y, unsigned int color )
{
	videodata[x+y*320] = color;
}

int main()
{
#if 0
	int x, y;
	for( y = 0; y < 30; y++ )
	{
		for( x = 0; x < 30; x++ )
		{
			PlotPixel( x + 30, y + 30, 0xff00ffff );
		}
	}
#endif
	lprint("Hello world from RV32 land.\n");
}


int _start()
{
	main();
	asm volatile( "EBREAK" );
}
