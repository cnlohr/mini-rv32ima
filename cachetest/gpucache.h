
			#define MAX_FCNT     512
			#define CACHE_BLOCKS 512
			#define CACHE_N_WAY  2

			static uint4 cachesetsdata[CACHE_BLOCKS];
			static uint  cachesetsaddy[CACHE_BLOCKS];
			static uint  cache_usage;

			// Only use if aligned-to-4-bytes.
			uint LoadMemInternalRB( uint ptr )
			{
				uint blockno = ptr / 16;
				uint blocknop1 = (ptr >> 4)+1;
				uint hash = (blockno % (CACHE_BLOCKS/CACHE_N_WAY)) * CACHE_N_WAY;
				uint4 block;
				uint ct = 0;
				uint i;
				for( i = 0; i < CACHE_N_WAY; i++ )
				{
					ct = cachesetsaddy[i+hash];
					if( ct == blocknop1 )
					{
						// Found block.
						uint4assign( block, cachesetsdata[i+hash] );
						return block[(ptr&0xf)>>2];
					}
					else if( ct == 0 )
					{
						// else, no block found. Read data.
						uint4assign( block, MainSystemAccess( blockno ) );
						return block[(ptr&0xf)>>2];
					}
				}

				if( i == CACHE_N_WAY )
				{
						// Reading after overfilled cache.
						// Need to panic here.
						// This should never ever happen.
						uint4assign( block, MainSystemAccess( blockno ) );
						return block[(ptr&0xf)>>2];
				}

				// Not arrivable.
				return 0;
			}


			// Store mem internal word (Only use if guaranteed word-alignment)
			void StoreMemInternalRB( uint ptr, uint val )
			{
				//printf( "STORE %08x %08x\n", ptr, val );
				uint blockno = ptr >> 4;
				uint blocknop1 = (ptr >> 4)+1;
				// ptr will be aligned.
				// perform a 4-byte store.
				uint hash = (blockno % (CACHE_BLOCKS/CACHE_N_WAY)) * CACHE_N_WAY;
				uint hashend = hash + CACHE_N_WAY;
				uint4 block;
				uint ct = 0;
				// Cache lines are 8-deep, by 16 bytes, with 128 possible cache addresses.
				for( ; hash < hashend; hash++ )
				{
					ct = cachesetsaddy[hash];
					if( ct == 0 ) break;
					if( ct == blocknop1 )
					{
						// Found block.
						cachesetsdata[hash][(ptr&0xf)>>2] = val;
						return;
					}
				}
				// NOTE: It should be impossible for i to ever be or exceed 1024.
				// We catch it early here.
				if( hash == hashend )
				{
					// We have filled a cache line.  We must cleanup without any other stores.
					cache_usage = MAX_FCNT;
					printf( "OVR Please Flush at %08x\n", ptr );
					fprintf( stderr, "ERROR: SERIOUS OVERFLOW %d\n", -1 );
					exit( -99 );
				}
				cachesetsaddy[hash] = blocknop1;
				uint4assign( block, MainSystemAccess( blockno ) );
				block[(ptr&0xf)>>2] = val;
				uint4assign( cachesetsdata[hash], block );
				// Make sure there's enough room to flush processor state (16 writes)
				cache_usage++;
				if( hash == hashend-1 )
				{
					cache_usage = MAX_FCNT;
				}
			}

			// NOTE: len does NOT control upper bits.
			uint LoadMemInternal( uint ptr, uint len )
			{
				uint lenx8mask = ((uint32_t)(-1)) >> (((4-len) & 3) * 8);
				uint remo = ptr & 3;
				if( remo )
				{
					if( len > 4 - remo )
					{
						// Must be split into two reads.
						uint ret0 = LoadMemInternalRB( ptr & (~3) );
						uint ret1 = LoadMemInternalRB( (ptr & (~3)) + 4 );

						uint ret = lenx8mask & ((ret0 >> (remo*8)) | (ret1<<((4-remo)*8)));


						uint check = 0;
						memcpy( &check, ram_image_shadow + ptr, len );
						if( check != ret )
						{
							fprintf( stderr, "Error check failed x (%08x != %08x) @ %08x\n", check, ret, ptr  );
							exit( -99 );
						}
				

						return ret;
					}
					else
					{
						// Can just be one.
						uint ret = LoadMemInternalRB( ptr & (~3) );
						ret = (ret >> (remo*8)) & lenx8mask;

						uint check = 0;
						memcpy( &check, ram_image_shadow + ptr, len );
						if( check != ret )
						{
							fprintf( stderr, "Error check failed y (%08x != %08x) @ %08x\n", check, ret, ptr  );
							exit( -99 );
						}

						return ret;
					}
				}
				uint ret = LoadMemInternalRB( ptr ) & lenx8mask;

				uint check = 0;
				memcpy( &check, ram_image_shadow + ptr, len );
				if( check != ret )
				{
					fprintf( stderr, "Error check failed z (%08x != %08x (%d) ) @ %08x\n", check, ret, len, ptr  );
					exit( -99 );
				}
				
				return LoadMemInternalRB( ptr ) & lenx8mask;
			}
			
			void StoreMemInternal( uint ptr, uint val, uint len )
			{
				memcpy( ram_image_shadow + ptr, &val, len );

				uint remo = (ptr & 3);
				uint remo8 = remo * 8;
				uint ptrtrunc = ptr - remo;
				uint lenx8mask = ((uint32_t)(-1)) >> (((4-len) & 3) * 8);
				if( remo + len > 4 )
				{
					// Must be split into two writes.
					// remo = 2 for instance, 
					uint val0 = LoadMemInternalRB( ptrtrunc );
					uint val1 = LoadMemInternalRB( ptrtrunc + 4 );
					uint mask0 = lenx8mask << (remo8);
					uint mask1 = lenx8mask >> (32-remo8);
					val &= lenx8mask;
					val0 = (val0 & (~mask0)) | ( val << remo8 );
					val1 = (val1 & (~mask1)) | ( val >> (32-remo8) );
					StoreMemInternalRB( ptrtrunc, val0 );
					StoreMemInternalRB( ptrtrunc + 4, val1 );
					//printf( "RESTORING: %d %d @ %d %d / %08x %08x -> %08x, %08x %08x -> %08x\n", remo, len, ptrtrunc, ptrtrunc+4, mask0, val, val0, mask1, val, val1 );
				}
				else if( len != 4 )
				{
					// Can just be one call.
					// i.e. the smaller-than-word-size write fits inside the word.
					uint valr = LoadMemInternalRB( ptrtrunc );
					uint mask = lenx8mask << remo8;
					valr = ( valr & (~mask) ) | ( ( val & lenx8mask ) << (remo8) );
					StoreMemInternalRB( ptrtrunc, valr );
					//printf( "RESTORING: %d %d @ %d / %08x %08x -> %08x\n", remo, len, ptrtrunc, mask, val, valr );
				}
				else
				{
					// Else it's properly aligned.
					StoreMemInternalRB( ptrtrunc, val );
				}
			}

			#define MINIRV32_POSTEXEC( a, b, c ) ;

			#define MINIRV32_CUSTOM_MEMORY_BUS
			uint MINIRV32_LOAD4( uint ofs ) { return LoadMemInternal( ofs, 4 ); }
			#define MINIRV32_STORE4( ofs, val ) { StoreMemInternal( ofs, val, 4 ); if( cache_usage >= MAX_FCNT ) icount = MAXICOUNT;}
			uint MINIRV32_LOAD2( uint ofs ) { uint tword = LoadMemInternal( ofs, 2 ); return tword; }
			uint MINIRV32_LOAD1( uint ofs ) { uint tword = LoadMemInternal( ofs, 1 ); return tword; }
			int MINIRV32_LOAD2_SIGNED( uint ofs ) { uint tword = LoadMemInternal( ofs, 2 ); if( tword & 0x8000 ) tword |= 0xffff0000;  return tword; }
			int MINIRV32_LOAD1_SIGNED( uint ofs ) { uint tword = LoadMemInternal( ofs, 1 ); if( tword & 0x80 )   tword |= 0xffffff00; return tword; }
			#define MINIRV32_STORE2( ofs, val ) { StoreMemInternal( ofs, val, 2 ); if( cache_usage >= MAX_FCNT ) icount = MAXICOUNT; }
			#define MINIRV32_STORE1( ofs, val ) { StoreMemInternal( ofs, val, 1 ); if( cache_usage >= MAX_FCNT ) icount = MAXICOUNT; }

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

