ENTRY(_start)
SECTIONS
{
	. = 0x00;

	.header : ALIGN( 16 )
	{
		LONG( __TEXT_BEGIN__ )
		LONG( __BSS_END__ - __TEXT_BEGIN__ )
		LONG( ( __TEXT_END__ - __TEXT_BEGIN__ ) )
		LONG( ( __DATA_END__ - __DATA_BEGIN__ ) )
		LONG( ( __BSS_END__ - __BSS_BEGIN__ ) )
	}

	. = 0x100;
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
	}

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
}
