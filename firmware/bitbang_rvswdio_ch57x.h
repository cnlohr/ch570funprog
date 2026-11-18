// Implementation of platform specific functions in funprog_rvswdio.h
// #include "funconfig.h"

#ifndef _BITBANG_RVSWDIO_CH570_H
#define _BITBANG_RVSWDIO_CH570_H

#if FUNCONF_SYSTEM_CORE_CLOCK == 60000000
#define SWDIO_T1 3
#define SWDIO_T2 4
#define SWDIO_T3 5
#define SWDIO_T4 6
#define SWDIO_T5 12
#define SWDIO_T6 15
#define SWD_T1 4
#define SWD_T2 8
#define SWD_T3 12
#define SWD_T4 1
#elif FUNCONF_SYSTEM_CORE_CLOCK == 100000000
#define SWDIO_T1 5
#define SWDIO_T2 7
#define SWDIO_T3 8
#define SWDIO_T4 10
#define SWDIO_T5 20
#define SWDIO_T6 24
// #define SWD_DELAY ADD_N_NOPS( 10 );
#define SWD_T1 7
#define SWD_T2 15
#define SWD_T3 20
#define SWD_T4 2
#endif

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

static inline void Send1BitSWIO(struct SWIOState * state)
{
	// 288-296ns low pulse. (Ideal period) - OK @ 2..15
	R32_PA_DIR |= PIN_SWD_MASK;
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T2 ); // Valid (no end break) 1 (100ns) .. 9 (500ns)
	R32_PA_SET = PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T3 ); // Valid 1-8
}

static inline void Send0BitSWIO(struct SWIOState * state)
{
	// 888-904ns - OK @ 32 (At TPERIOD=48) - @48 (At TPERIOD=56)
	// Oddly, also works at =20 for TPERIOD=56
	R32_PA_DIR |= PIN_SWD_MASK;
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T6 ); // Valid 9 (500ns) .. 40 (2us)
	R32_PA_SET = PIN_SWD_MASK;	
	Delay_Tiny_Inline( SWDIO_T4 ); // Valid 2+
}

static inline int ReadBitSWIO(struct SWIOState * state)
{
	R32_PA_CLR = PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T1 ); // valid 1-8 (was 2)
	R32_PA_DIR &= ~PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T3 ); // valid 1-8 (was 5)
	int r = !!(R32_PA_PIN&PIN_SWD_MASK);
	R32_PA_SET = PIN_SWD_MASK;
	Delay_Tiny_Inline( SWDIO_T5 );
	R32_PA_DIR |= PIN_SWD_MASK;
	return r;
}

static void IssueBuffer(void) {
	Delay_Us(1);
}

static void FinishBuffer(void)
{
	__enable_irq();
}

////////////////////////////////////////////////////////////////
// For SWD
////////////////////////////////////////////////////////////////

static void SendBitRVSWD( struct SWIOState * state, int val )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return; // Don't touch PA2/3 pins when UART is enabled
#endif
	// Assume:
	// SWD is in indeterminate state.
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
	// funPinMode( PIN_SWD_MASK, GPIO_CFGLR_OUT_10Mhz_PP );
	// *(&R32_PA_PD_DRV)  |= PIN_SWD_MASK;
	*(&R32_PA_DIR )    |= PIN_SWD_MASK;

	Delay_Tiny_Inline(state->t1coeff);
	funDigitalWriteMask( PIN_SWC_MASK, 1 );
	Delay_Tiny_Inline(state->t1coeff);
}

static int ReadBitRVSWD( struct SWIOState * state )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return 0;
#endif
	// funPinMode( PIN_SWD, GPIO_CFGLR_IN_FLOAT );
	// *(&R32_PA_PD_DRV)  &= ~PIN_SWD_MASK;
	*(&R32_PA_DIR )    &= ~PIN_SWD_MASK;
	// *(&R32_PA_PD_DRV)      |= PIN_SWD_MASK;

	// funDigitalWriteMask( PIN_SWD_MASK, 0 );
	funDigitalWriteMask( PIN_SWC_MASK, 0 );
	Delay_Tiny_Inline(state->t1coeff);
	int r = !!(funDigitalReadMask( PIN_SWD_MASK ));
	funDigitalWriteMask( PIN_SWC_MASK, 1 );
	Delay_Tiny_Inline(state->t1coeff);
	return r;
}

static void SWDStop( struct SWIOState * state )
{
	funDigitalWriteMask( PIN_SWC_MASK, 0 );
	Delay_Tiny_Inline(SWD_T4);
	funDigitalWriteMask( PIN_SWD_MASK, 0 );
	// funPinMode( PIN_SWD_MASK, GPIO_CFGLR_OUT_50Mhz_PP );
	// *(&R32_PA_PD_DRV)  |= PIN_SWD_MASK;
	*(&R32_PA_DIR )    |= PIN_SWD_MASK;

	Delay_Tiny_Inline(SWD_T4);
	funDigitalWriteMask( PIN_SWC_MASK, 1 );

	Delay_Tiny_Inline(SWD_T4);
	funDigitalWriteMask( PIN_SWD_MASK, 1 );

	Delay_Us(2); // Sometimes 2 is too short.
}

static void RVFinishRegop( struct SWIOState * state )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return;
#endif
	ReadBitRVSWD( state ); // ???
	ReadBitRVSWD( state ); // ???
	ReadBitRVSWD( state ); // ???
	SendBitRVSWD( state, 1 ); // 0 for register, 1 for value
	SendBitRVSWD( state, 0 ); // ??? Seems to have something to do with halting?

	SWDStop( state );
	__enable_irq();
}

#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
static void SWDWakeUP( struct SWIOState * state )
{
	__disable_irq();
	Delay_Tiny_Inline(SWD_T4);
	for (int i = 0; i < 100; i++) {
		funDigitalWriteMask( PIN_SWC_MASK, 0 );
		Delay_Tiny_Inline(state->t1coeff);
		funDigitalWriteMask( PIN_SWC_MASK, 1 );
		Delay_Tiny_Inline(state->t1coeff);
	}

	SWDStop( state );
	__enable_irq();
}

static void SWDLongOP( struct SWIOState * state, uint8_t reg, uint32_t * value, uint8_t op )
{
	uint32_t mask;
	// Start
	funDigitalWriteMask( PIN_SWD_MASK, 0 );
	Delay_Tiny_Inline(SWD_T4);

	for( mask = 1<<6; mask; mask >>= 1 )
	{
		int v = !!(reg & mask);
		SendBitRVSWD( state, v );
	}
	for( mask = 1<<31; mask; mask >>= 1 )
	{
		int v = !!(*value & mask);
		SendBitRVSWD( state, v );
	}

	SendBitRVSWD( state, op&2 );
	SendBitRVSWD( state, op&1 );
	// SendBitRVSWD( state, 0 ); // Parity is always 0
	ReadBitRVSWD( state );

	for( int i = 0; i < 7; i++ )
	{
		ReadBitRVSWD( state );
	}

	uint32_t rval = 0;
	for( int i = 0; i < 32; i++ )
	{
		rval <<= 1;
		int r = ReadBitRVSWD( state );
		if( r == 1 ) rval |= 1;
	}

	ReadBitRVSWD( state );
	ReadBitRVSWD( state );

	if ( op == 2 ) ReadBitRVSWD( state );
	else SendBitRVSWD( state, 1 );

	SWDStop( state );
	__enable_irq();

	if ( op == 1 ) memcpy( value, &rval, 4 );
}
#endif

void MCFWriteReg32( struct SWIOState * state, uint8_t command, uint32_t value )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return;
#endif

	if( state->opmode == 1 )
	{
		__disable_irq();
		//printf( "WRITEREG: %02x %08x\n", command, value );
		Send1BitSWIO( state );
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}
		Send1BitSWIO( state );
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			if( value & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}

		IssueBuffer();
		FinishBuffer();
	}
	else if( state->opmode == 2 )
	{
		__disable_irq();
		uint32_t mask;
		funDigitalWriteMask( PIN_SWD_MASK, 0 );
		Delay_Tiny_Inline(SWD_T4);
		int parity = 1;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, 1 ); // Write = Set high
		SendBitRVSWD( state, parity );
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // Seems only need to be set for first transaction (We are ignoring that though)
		ReadBitRVSWD( state ); // ???
		SendBitRVSWD( state, 0 ); // 0 for register, 1 for value.
		SendBitRVSWD( state, 0 ); // ???  Seems to have something to do with halting.

		parity = 0;
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			int v = !!(value & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, parity );
		RVFinishRegop( state );
	}
#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
	else if( state->opmode == 3 )
	{
		__disable_irq();
		uint32_t data = value;
		SWDLongOP( state, command, &data, 2 );
	}
#endif
}

// returns 0 if no error, otherwise error.
int MCFReadReg32( struct SWIOState * state, uint8_t command, uint32_t * value )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return 0;
#endif

	if( state->opmode == 1 )
	{
		__disable_irq();
		Send1BitSWIO( state );
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}
		Send0BitSWIO( state );
		uint32_t rv = 0;
		int i;
		for( i = 0; i < 32; i++ )
		{
			rv = (rv << 1) | ReadBitSWIO( state );
		}
		ReadBitSWIO( state ); // check bit.
		IssueBuffer();
		FinishBuffer();

		memcpy( value, &rv, 4 );
	}
	else if( state->opmode == 2 )
	{
		__disable_irq();
		int mask;
		funDigitalWriteMask( PIN_SWD_MASK, 0 );
		Delay_Tiny_Inline(SWD_T4);
		int parity = 0;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, 0 ); // Read = Set low
		SendBitRVSWD( state, parity );
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // ???
		SendBitRVSWD( state, 0 ); // 0 for register, 1 for value
		SendBitRVSWD( state, 0 ); // ??? Seems to have something to do with halting?

		uint32_t rval = 0;
		int i;
		parity = 0;
		for( i = 0; i < 32; i++ )
		{
			rval <<= 1;
			int r = ReadBitRVSWD( state );
			if( r == 1 )
			{
				rval |= 1;
				parity ^= 1;
			}
			if( r == 2 )
			{
				RVFinishRegop( state );
				return -1;
			}
		}
		memcpy( value, &rval, 4 );

		if( ReadBitRVSWD( state ) != parity )
		{
			//BB_PRINTF_DEBUG( "Parity Failed\n" );
			RVFinishRegop( state );
			return -1;
		}
		RVFinishRegop( state );
	}
#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
	else if( state->opmode == 3 )
	{
		__disable_irq();
		uint32_t reply = 0;
		SWDLongOP( state, command, &reply, 1 );
		*value = reply;
	}
#endif
	return 0;
}

void ConfigureIOForRVSWD( struct SWIOState * state )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return;
#endif
	funDigitalWrite( PIN_SWC, 1 );
	funPinMode( PIN_SWC, GPIO_CFGLR_OUT_10Mhz_PP );
	funDigitalWrite( PIN_SWD, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
}

void ConfigureIOForRVSWIO( struct SWIOState * state )
{
#if defined(HW_VERSION) && HW_VERSION==1
	if( uart_enabled ) return;
#endif
	funDigitalWrite( PIN_SWD, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
}

static void SetProtocolSpeed( struct SWIOState * state, enum ProtocolSpeed speed )
{
	switch( speed )
	{
		case PROTOCOL_SPEED_DEFAULT:
			state->t1coeff = SWD_T1;
			break;

		case PROTOCOL_SPEED_MEDIUM:
			state->t1coeff = SWD_T2;
			break;

		case PROTOCOL_SPEED_LOW:
			state->t1coeff = SWD_T3;
			break;
	}
	state->protocol_speed = speed;
}

#endif
