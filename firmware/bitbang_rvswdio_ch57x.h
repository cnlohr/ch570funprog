// Implementation of platform specific functions in bitbang_rvswdio.h
//
// If you want the most up-to-date version of this, it will be stored in rv003usb.
//

#define SWD_DELAY ADD_N_NOPS( 5 );

#ifndef _BITBANG_RVSWDIO_CH570_H
#define _BITBANG_RVSWDIO_CH570_H

#define GPIO_SetBitsMask(pin)             (*(&R32_PA_SET ) =  ((pin)))
#define GPIO_ResetBitsMask(pin)           (*(&R32_PA_CLR ) =  ((pin)))
#define GPIO_ReadPortPinMask(pin)         ((*(&R32_PA_PIN)) & (pin))
#define funDigitalReadMask(pin)           !!GPIO_ReadPortPinMask(pin)
#define funDigitalWriteMask( pin, value ) do{ if((value)==FUN_HIGH){GPIO_SetBitsMask(pin);} else if((value)==FUN_LOW){GPIO_ResetBitsMask(pin);} }while(0)

static inline void Delay_Tiny_Inline( int n ) {
	asm volatile( "\
		1: \
		c.addi %[n], -1\n\
		c.bnez %[n], 1b" : [n]"+r"(n) );
}

static inline void Send1Bit(void) __HIGH_CODE;
static inline void Send0Bit(void) __HIGH_CODE;
static inline int ReadBit(void) __HIGH_CODE;
static void FinishBuffer(void);
static void IssueBuffer(void);

void ConfigureIOForRVSWD(void);
void ConfigureIOForRVSWIO(void);

static inline void Send1Bit(void)
{
	// 288-296ns low pulse. (Ideal period) - OK @ 2..15
	R32_PA_DIR |= PIN_SWD_MASK;
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( 4 ); // Valid (no end break) 1 (100ns) .. 9 (500ns)
	R32_PA_SET = PIN_SWD_MASK;
	Delay_Tiny_Inline( 3 ); // Valid 1-8
}

static inline void Send0Bit(void)
{
	// 888-904ns - OK @ 32 (At TPERIOD=48) - @48 (At TPERIOD=56)
	// Oddly, also works at =20 for TPERIOD=56
	R32_PA_DIR |= PIN_SWD_MASK;
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( 15 ); // Valid 9 (500ns) .. 40 (2us)
	R32_PA_SET = PIN_SWD_MASK;	
	Delay_Tiny_Inline( 6 ); // Valid 2+
}

static inline int ReadBit(void)
{
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( 3 ); // valid 1-8 (was 2)
	R32_PA_DIR &= ~PIN_SWD_MASK;
	Delay_Tiny_Inline( 5 ); // valid 1-8 (was 5)
	int r = !!(R32_PA_PIN&PIN_SWD_MASK);
	R32_PA_SET = PIN_SWD_MASK;
	Delay_Tiny_Inline( 12 );
	R32_PA_DIR |= PIN_SWD_MASK;
	return r;
}

static void IssueBuffer(void) { }

static void FinishBuffer(void)
{
	__enable_irq();
}

////////////////////////////////////////////////////////////////
// For SWD
////////////////////////////////////////////////////////////////

static void SendBitRVSWD( int val )
{
	// Assume:
	// SWD is in indeterminte state.
	// SWC is HIGH
	funDigitalWriteMask( PIN_SWC_MASK, 0 );
	if( val )
	{
		funDigitalWriteMask( PIN_SWD_MASK, 1 );
	}
	else
	{
		funDigitalWriteMask( PIN_SWD_MASK, 0 );
	}
	//funPinMode( PIN_SWD_MASK, GPIO_CFGLR_OUT_10Mhz_PP );
	*(&R32_PA_PD_DRV)  |= PIN_SWD_MASK;
	*(&R32_PA_DIR )    |= PIN_SWD_MASK;

	SWD_DELAY;
	funDigitalWriteMask( PIN_SWC_MASK, 1 );
	SWD_DELAY;
}

static int ReadBitRVSWD( void )
{
	//funPinMode( PIN_SWD_MASK, GPIO_CFGLR_IN_PUPD );
	*(&R32_PA_PD_DRV)  &= ~PIN_SWD_MASK;
	*(&R32_PA_DIR )    &= ~PIN_SWD_MASK;
	*(&R32_PA_PU)      |= PIN_SWD_MASK;

	funDigitalWriteMask( PIN_SWD_MASK, 1 );
	funDigitalWriteMask( PIN_SWC_MASK, 0 );
	SWD_DELAY;
	int r = !!(funDigitalReadMask( PIN_SWD_MASK ));
	funDigitalWriteMask( PIN_SWC_MASK, 1 );
	SWD_DELAY;
	return r;
}

static void RVFinishRegop(void)
{
	ReadBitRVSWD( ); // ???
	ReadBitRVSWD( ); // ???
	ReadBitRVSWD( ); // ???
	SendBitRVSWD( 1 ); // 0 for register, 1 for value
	SendBitRVSWD( 0 ); // ??? Seems to have something to do with halting?


	funDigitalWriteMask( PIN_SWC_MASK, 0 );
	SWD_DELAY;
	funDigitalWriteMask( PIN_SWD_MASK, 0 );
//	funPinMode( PIN_SWD_MASK, GPIO_CFGLR_OUT_50Mhz_PP );
	*(&R32_PA_PD_DRV)  |= PIN_SWD_MASK;
	*(&R32_PA_DIR )    |= PIN_SWD_MASK;

	SWD_DELAY;
	funDigitalWriteMask( PIN_SWC_MASK, 1 );
	SWD_DELAY;
	funDigitalWriteMask( PIN_SWD_MASK, 1 );

	Delay_Us(2); // Sometimes 2 is too short.
	__enable_irq();
}

void MCFWriteReg32( struct SWIOState * state, uint8_t command, uint32_t value )
{
	__disable_irq();
	if( state->opmode == 1 )
	{
		//printf( "WRITEREG: %02x %08x\n", command, value );
		Send1Bit();
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1Bit();
			else
				Send0Bit();
		}
		Send1Bit( );
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			if( value & mask )
				Send1Bit();
			else
				Send0Bit();
		}

		IssueBuffer();
		FinishBuffer();
	}
	else if( state->opmode == 2 )
	{
		uint32_t mask;
		funDigitalWriteMask( PIN_SWD_MASK, 0 );
		SWD_DELAY;
		int parity = 1;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( v );
		}
		SendBitRVSWD( 1 ); // Write = Set high
		SendBitRVSWD( parity );
		ReadBitRVSWD( ); // ???
		ReadBitRVSWD( ); // Seems only need to be set for first transaction (We are ignoring that though)
		ReadBitRVSWD( ); // ???
		SendBitRVSWD( 0 ); // 0 for register, 1 for value.
		SendBitRVSWD( 0 ); // ???  Seems to have something to do with halting.

		parity = 0;
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			int v = !!(value & mask);
			parity ^= v;
			SendBitRVSWD( v );
		}
		SendBitRVSWD( parity );
		RVFinishRegop();
	}
}

// returns 0 if no error, otherwise error.
int MCFReadReg32( struct SWIOState * state, uint8_t command, uint32_t * value )
{
	__disable_irq();
	if( state->opmode == 1 )
	{
		Send1Bit();
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1Bit();
			else
				Send0Bit();
		}
		Send0Bit( );
		uint32_t rv = 0;
		int i;
		for( i = 0; i < 32; i++ )
		{
			rv = (rv << 1) | ReadBit();
		}
		ReadBit(); // check bit.
		IssueBuffer();
		FinishBuffer();

		memcpy( value, &rv, 4 );
	}
	else if( state->opmode == 2 )
	{
		int mask;
		funDigitalWriteMask( PIN_SWD_MASK, 0 );
		SWD_DELAY;
		int parity = 0;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( v );
		}
		SendBitRVSWD( 0 ); // Read = Set low
		SendBitRVSWD( parity );
		ReadBitRVSWD( ); // ???
		ReadBitRVSWD( ); // ???
		ReadBitRVSWD( ); // ???
		SendBitRVSWD( 0 ); // 0 for register, 1 for value
		SendBitRVSWD( 0 ); // ??? Seems to have something to do with halting?


		uint32_t rval = 0;
		int i;
		parity = 0;
		for( i = 0; i < 32; i++ )
		{
			rval <<= 1;
			int r = ReadBitRVSWD( );
			if( r == 1 )
			{
				rval |= 1;
				parity ^= 1;
			}
			if( r == 2 )
			{
				RVFinishRegop();
				return -1;
			}
		}
		memcpy( value, &rval, 4 );

		if( ReadBitRVSWD( ) != parity )
		{
			//BB_PRINTF_DEBUG( "Parity Failed\n" );
			RVFinishRegop();
			return -1;
		}

		RVFinishRegop();
	}
	return 0;
}

void ConfigureIOForRVSWD(void)
{
	funDigitalWrite( PIN_SWC, 1 );
	funPinMode( PIN_SWC, GPIO_CFGLR_OUT_10Mhz_PP );
	funDigitalWrite( PIN_SWD, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
}

void ConfigureIOForRVSWIO(void)
{
	funDigitalWrite( PIN_SWD, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
}

#endif


