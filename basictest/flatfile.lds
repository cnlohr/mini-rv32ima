__heap_size = 0x200;
__stack_size = 0x800;

ENTRY(_start)


PHDRS
{
    headers PT_PHDR FILEHDR PHDRS;
    code PT_LOAD FILEHDR;
}

SECTIONS
{
	. = 0x00;

	.header : ALIGN( 16 )
	{
		/* Registers */
		LONG( 0 ) /* Zero */
		LONG( 0 ) /* Return Address */
		LONG( _sstack ) /* Stack Pointer */
		LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )
		LONG( 0 ) LONG( 0 ) LONG( 0 ) LONG( 0 )

		/* CPU State */
		LONG( _start ) /* PC */
		LONG( 0 )
		LONG( 0 )
		LONG( 0 )
	}

	.text : ALIGN(16) {
		__TEXT_BEGIN__ = .;
		*(.initial_jump)
		*(.entry.text)
		*(.init.literal)
		*(.init)
		*(.literal .text .literal.* .text.* .stub)
		*(.out_jump.literal.*)
		*(.out_jump.*)
		__TEXT_END__ = .;
	} :code

	.data : ALIGN(16) {
		__DATA_BEGIN__ = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.dynbss)
		*(.data)
		*(.data.*)
		__DATA_END__ = .;
	}

	.bss : ALIGN( 16 ) {
		__BSS_BEGIN__ = .;
		*(.bss) /* Tricky: BSS needs to be allocated but not sent. GCC Will not populate these for calculating data size */
		*(.bss.*)
		__BSS_END__ = .;
	}

	.heap : ALIGN( 16 ) {
		_sheap = .;
		. = . + __heap_size;
		_eheap = .;
	}

	.stack : ALIGN( 16 ) {
		_estack = .;
		. = . + __stack_size;
		_sstack = .;
	}

	PROVIDE( videodata = 0xb0000 );
	PROVIDE( uarthead = 0xfb000 );
	PROVIDE( uarttail = 0xfb004 );
	PROVIDE( uartbuffer = 0xfb100 );
}


