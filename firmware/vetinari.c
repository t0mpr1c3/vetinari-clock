/******************************************************************************
*
*      Project     : Vetinari Clock
*      Target CPU  : MSP430G2231
*      Compiler    : gcc-msp430
*      Copyright   : None
*      Version     : $Revision: 1A
*      \file         vetinari.c
*      \brief        The Vetinari clock is from a book series known as Discworld, where
*      Lord Verinari has a clock in his waiting room which has an irregular tick.  The
*      idea of the clock is to add a sense of unease and anxiety to anyone in the waiting
*      room since their brain doesn't filter out the ticks like a normal clock.
*
*      To accomplish this task on a 430, we create an array of possible time frames to
*      tick the clock, and parse through it at 4Hz.  The array is 32 entries long, so it
*      equates to 32 seconds in the real world.  By randomly setting 32 of the elements high,
*      we create a timing sequence. A high element will generate a tick of the clock.  This
*      means a second on the clock can be as little as 250ms, or as long as 24 seconds, and
*      still keep accurate time.
*
*****************************************************************************/

#include "msp430.h"
#include <legacymsp430.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Defines - Too lazy to put these in a header, maybe later */
/* How many clock cycles cycles to keep the io high
 * This will chgange depending on the model of clock movement used */
#define ENERGISE_TIME	0xC000

/* IO Mapping */
#define COIL_OUT		P1OUT
#define COIL_DIR		P1DIR

/* H-Bridge control pins */
#define COIL_ONE		BIT0 + BIT1
#define COIL_TWO		BIT2 + BIT3

/* Sequence length in seconds (must be power of 2)*/
#define SEQUENCE_LENGTH			64

/* Bit array of timing sequence */
static uint8_t timingSequence[SEQUENCE_LENGTH / 2];

/*
 * send a pulse to the clock coil
 */
void pulseClock(void)
{
	/* the polarity on the coil must swap each time it ticks, so we need to keep track of it */
	static uint8_t polarity;

	if (polarity == 0)
	{
		COIL_OUT = COIL_ONE;
		__delay_cycles(ENERGISE_TIME);
		COIL_OUT = 0;
		polarity = 1;
	}
	else
	{
		COIL_OUT = COIL_TWO;
		__delay_cycles(ENERGISE_TIME);
		COIL_OUT = 0;
		polarity = 0;
	}
}

/*
 * Using a LFSR, generate a "random" 16-bit number. This was snagged from wikipedia
 */
uint16_t get_rand(uint16_t in)
{
	uint16_t lfsr = in;
	static unsigned bit;
	static unsigned period = 0;

	/* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
	bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
	lfsr =  (lfsr >> 1) | (bit << 15);
	++period;

	return(lfsr);
}

/*
 * Reset the clock sequence, runs every SEQUENCE_LENGTH seconds
 */
void ResetSequence(void)
{
	uint16_t i = SEQUENCE_LENGTH;
	uint16_t location;
	static uint16_t feedback = 0xACE1;

	/* Zero out all elements */
	memset(&timingSequence, 0x0, sizeof(timingSequence));

	/* The array needs to have SEQUENCE_LENGTH random elements set high.
	 * To do this we generate SEQUENCE_LENGTH different random numbers to use as indexes for the bits that will be set high.
	 * If the index is already set, we just discard that number and try for a new one.
	 */
	do {
		/* get a new random number */
		feedback = get_rand(feedback);

		/* We only want the lower bits of the random number to fit the length of the element array
		 * The 16-bit number is still used so that we get a longer
		 * chain in the LFSR before it repeats */
		location = ((uint16_t) (SEQUENCE_LENGTH * 4 - 1)) & feedback;

		/* If the random location has already been set high, we ignore the random number */
		uint8_t byte = (location >> 3);
		uint8_t bit = (1 << (location & 0x07));
		if ((timingSequence[byte] & bit) == 0)
		{
			/* Other wise we set the element */
			timingSequence[byte] |= bit;
			/* and decrement the counter */
			i--;
		}

	/* This needs to be done SEQUENCE_LENGTH times */
	} while (i);
}

/*
 * I setup the MSP, and run the main program loop
 */
int main(void) {

	/* counter to determine when SEQUENCE_LENGTH seconds have past */
	uint16_t counter = 0;

	/* 12.5 pF watch crystal */
	BCSCTL3 |= XCAP_3;

	/* At this point you are supposed to let the crystal period settle */
	//__delay_cycles(55000);

	/* setup watchdog for 0.25s interval
	 * WDT ISR set at watchdog_timer(void)*/
	WDTCTL = WDT_ADLY_250;
	IE1 |= WDTIE;

	/* Enable Osc Fault */
	IE1 |= OFIE;

	/* Setup IO */
	COIL_DIR = COIL_ONE + COIL_TWO;

	/* Initialize the pulse sequence */
	ResetSequence();

	/* Enter LPM3 w/interrupt */
	_BIS_SR(LPM3_bits + GIE);

	while (1)
	{
		/* If this element of the sequence is high, we need to tick the clock */
		uint8_t byte = (counter >> 3);
		uint8_t bit = (1 << (counter & 0x07));
		if ((timingSequence[byte] & bit) > 0)
		{
			pulseClock();
		}

		/* Increment the counter to get us closer to SEQUENCE_LENGTH sec */
		counter++;

		/* The WDT runs at 4Hz, so SEQUENCE_LENGTH sec at equates to 4 * SEQUENCE_LENGTH ISR firings
		 * At the SEQUENCE_LENGTH sec mark, we want to reset the counter and generate a new pulse sequence */
		if (counter == 4 * SEQUENCE_LENGTH)
		{
			counter = 0;
			ResetSequence();
		}

		/* Enter LPM3 w/interrupt */
		_BIS_SR(LPM3_bits + GIE);
		/* Once the WDT interrupt fires, it will return here looping back to the start of the while loop */
	}

	return 0;
}

/*
 * The watchdog timer is actually useful
 */
//#pragma vector=WDT_VECTOR
//__interrupt void watchdog_timer (void)
interrupt (WDT_VECTOR) watchdog_timer (void)
{
	/* Clear LPM3 bits from 0(SR)
	 * This will send us back to the main while loop */
	_BIC_SR_IRQ(LPM3_bits);
}

/*
 * Just in case
 */
//#pragma vector=NMI_VECTOR
//__interrupt void nmi_ (void)
interrupt (NMI_VECTOR) nmi_ (void)
{
	uint16_t i;
	do
	{
		IFG1 &= ~OFIFG;                         // Clear OSCFault flag
		for (i = 0xFFF; i > 0; i--);            // Time for flag to set
	}
	while (IFG1 & OFIFG);                     // OSCFault flag still set?
	IE1 |= OFIE;                              // Enable Osc Fault
}
