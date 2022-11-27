#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

extern uint32_t SYSCON;
extern uint32_t TIMERL;
extern void asm_demo_func();

static void lprint( const char * s )
{
	asm volatile( "csrrw x0, 0x138, %0\n" : : "r" (s));
}

static void pprint( intptr_t ptr )
{
	asm volatile( "csrrw x0, 0x137, %0\n" : : "r" (ptr));
}

static void nprint( intptr_t ptr )
{
	asm volatile( "csrrw x0, 0x136, %0\n" : : "r" (ptr));
}

static inline uint32_t get_cyc_count() {
	uint32_t ccount;
	asm volatile("csrr %0, 0xC00":"=r" (ccount));
	return ccount;
}

int main()
{
	lprint("\n");
	lprint("Hello world from RV32 land.\n");
	lprint("main is at: ");
	pprint( (intptr_t)main );
	lprint("\n");
	asm_demo_func();
	lprint("\n");

	// Wait a while.
	uint32_t cyclecount_initial = get_cyc_count();
	uint32_t timer_initial = TIMERL;

	volatile int i;
	for( i = 0; i < 1000000; i++ )
	{
		asm volatile( "nop" );
	}

	// Gather the wall-clock time and # of cycles
	uint32_t cyclecount = get_cyc_count() - cyclecount_initial;
	uint32_t timer = TIMERL - timer_initial;

	lprint( "Processor effective speed: ");
	nprint( cyclecount / timer );
	lprint( " Mcyc/s\n");

	lprint("\n");
	SYSCON = 0x5555; // Power off
}

