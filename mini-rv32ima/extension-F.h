#ifndef _EXTENSION_F_H
#define _EXTENSION_F_H

#define MINIRV32IMA_ADDITIONAL_HEADER \
	float regf[32]; \
	uint32_t floatflags;

#define FREGSET( x, val ) { state->regf[x] = val; }
#define FREG( x ) state->regf[x]
#define COERSE_TO_FLOAT( x ) (*((float*)&x))
#define COERSE_TO_UINT( x ) (*((uint32_t*)&x))
#define FSQRT( x ) fsqrt( x )

#define MINIRV32IMA_OTHER_OPCODES \
	case 0x07: /*FLW*/ \
		if( ( ir >> 12 ) & 0x7 == 2 ) \
		{ \
			uint32_t rv = MINIRV32_LOAD4( rsval ); \
			float rval = COERSE_TO_FLOAT( rv ); \
			FREGSET( rdid, rval ); \
			rdid = 0; /* Don't use regular register saving */ \
		} \
		else \
			trap = (2+1); \
		break; \
	case 0x27: /*FSW*/ \
		if( ( ir >> 12 ) & 0x7 == 2 ) \
		{ \
			uint32_t rs1 = REG((ir >> 15) & 0x1f); \
			uint32_t rs2 = COERSE_TO_UINT( FREG((ir >> 20) & 0x1f) ); \
			uint32_t addy = ( ( ir >> 7 ) & 0x1f ) | ( ( ir & 0xfe000000 ) >> 20 ); \
			if( addy & 0x800 ) addy |= 0xfffff000; \
			addy += rs1 - MINIRV32_RAM_IMAGE_OFFSET; \
			MINIRV32_STORE4( addy, rs2 ); \
			rdid = 0; /* Don't register save */ \
		} \
		else \
			trap = (2+1); \
		break; \
	case 0x43: /*1000011, FMADD.S*/ \
		float rs1 = FREG((ir >> 15) & 0x1f); \
		float rs2 = FREG((ir >> 20) & 0x1f); \
		float rs3 = FREG((ir >> 27) & 0x1f); \
		int rm = (ir>>12)&7; \
		FREGSET( rdid, rs1*rs2+rs3 ); \
		rdid = 0; /* Don't register save */ \
		break; \
	case 0x47: /*1000111, FMSUB.S*/ \
		float rs1 = FREG((ir >> 15) & 0x1f); \
		float rs2 = FREG((ir >> 20) & 0x1f); \
		float rs3 = FREG((ir >> 27) & 0x1f); \
		int rm = (ir>>12)&7; \
		FREGSET( rdid, rs1*rs2-rs3 ); \
		rdid = 0; /* Don't register save */ \
		break; \
	case 0x4b: /*1001011, FNMSUB.S*/ \
		float rs1 = FREG((ir >> 15) & 0x1f); \
		float rs2 = FREG((ir >> 20) & 0x1f); \
		float rs3 = FREG((ir >> 27) & 0x1f); \
		int rm = (ir>>12)&7; \
		FREGSET( rdid, -rs1*rs2+rs3 ); \
		rdid = 0; /* Don't register save */ \
		break; \
	case 0x4f: /*1001111, FNMADD.S*/ \
		float rs1 = FREG((ir >> 15) & 0x1f); \
		float rs2 = FREG((ir >> 20) & 0x1f); \
		float rs3 = FREG((ir >> 27) & 0x1f); \
		int rm = (ir>>12)&7; \
		FREGSET( rdid, -rs1*rs2-rs3 ); \
		rdid = 0; /* Don't register save */ \
		break; \
	case 0x53: /*1010011, Regular float operation*/ \
		float rs1 = FREG((ir >> 15) & 0x1f); \
		float rs2 = FREG((ir >> 20) & 0x1f); \
		int rm = (ir>>12)&7; \
		int subop = ir>>25; \
		switch( subop ) \
		{ \
		case 0x00: FREGSET( rdid, rs1 + rs2 ); break; /*0000000 FADD.S*/ \
		case 0x04: FREGSET( rdid, rs1 - rs2 ); break; /*0000100 FSUB.S*/ \
		case 0x08: FREGSET( rdid, rs1 * rs2 ); break; /*0001000 FMUL.S*/ \
		case 0x0c: FREGSET( rdid, rs1 / rs2 ); break; /*0001100 FDIV.S*/ \
		case 0x2c: FREGSET( rdid, FSQRT( rs1 ) ); break; /*0101100 FSQRT.S*/ \
#error TODO: Pick up here
		case 0x10: FREGSET( rdid, rs1  rs2 ); break; /*0010000 FSGNJ.S, FSGNJN.S, FSGNJX.S */ \
		case 0x14: FREGSET( rdid, rs1 + rs2 ); break; /*0010100 FMIN.S, FMAX.S*/ \
		case 0x60: FREGSET( rdid, rs1 + rs2 ); break; /*1100000 FCVT.W.S, FCVT.WU.S */ \
		case 0x50: FREGSET( rdid, rs1 + rs2 ); break; /*1010000 FEQ.S, FLT.S , FLE.S */ \
		case 0x70: FREGSET( rdid, rs1 + rs2 ); break; /*1110000 FCLASS.S */ \
		case 0x68: FREGSET( rdid, rs1 + rs2 ); break; /*1101000 FCVT.S.W, FCVT.S.WU */ \
		case 0x78: FREGSET( rdid, rs1 + rs2 ); break; /*1111000 FMV.W.X */ \
		} \
		rdid = 0; \
		break; \
#endif

