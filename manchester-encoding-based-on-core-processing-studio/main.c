/*
\file main.c

\brief Main source file.

(c) 2020 Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and any
derivatives exclusively with Microchip products. It is your responsibility to comply with third party
license terms applicable to your use of third party software (including open source software) that
may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
FOR A PARTICULAR PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
SOFTWARE.
*/

#include <atmel_start.h>
#include <util/delay.h>

/* Select Manchester encoding convention */
#define ENCODING_G_E_THOMAS
#ifndef ENCODING_G_E_THOMAS
#define ENCODING_IEEE
#endif

/* Select baud rate */
#define BAUD_RATE 50000UL

/* Transfer data setup */
#define START_BYTE 0x55
#define TRANSMIT_BUFFER_SIZE 255
volatile uint8_t transmit_buffer[TRANSMIT_BUFFER_SIZE] = {START_BYTE};
volatile uint8_t transmit_buffer_length                = 0;
volatile uint8_t sending_in_progress                   = 0;

/* Defining an example packet */
#define TRANSMIT_EXAMPLE_SIZE 18
const uint8_t transmit_example[TRANSMIT_EXAMPLE_SIZE] = "Hello Manchester!";

/*
 * TCB0 initialization
 *
 * Already mostly set up by ATMEL Start. This function only
 * configures the CCMP value to get the chosen baud rate and
 * enables the peripheral.
 */
void TCB0_init(void)
{
	/* Calculate CCMP value */
	TCB0.CCMP = (uint16_t)(((float)F_CPU) / (2 * BAUD_RATE) + 0.5);

	/* Enable TCB0 */
	TCB0.CTRLA |= TCB_ENABLE_bm;
}

/*
 * Send Manchester encoded data. If data is currently being transmitted
 * the new data will not be sent and the function returns zero.
 */
uint8_t send_encoded_data(const uint8_t *transmit_data, uint8_t num_bytes)
{
	if (!sending_in_progress) {
		for (uint8_t i = 0; i < num_bytes; i++) {
			transmit_buffer[i] = transmit_data[i];
		}
		transmit_buffer_length = num_bytes + 1;
		sending_in_progress    = 1;
		return 1;
	} else {
		return 0;
	}
};

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	/* Additional initialization */
	TCB0_init();

/* Ensure that the output has the correct initial value */
#ifdef ENCODING_IEEE
	PORTA.OUTSET = PIN4_bm;
#endif

	/* Main loop */
	while (1) {
		while (!send_encoded_data(transmit_example, TRANSMIT_EXAMPLE_SIZE))
			;

		/* Add a delay after a packet is sent before re-enabling the TCB */
		if ((sending_in_progress) && !(TCB0.CTRLA & TCB_ENABLE_bm)) {
			/* Ensure a sufficient timeout period between packets */
			for (uint8_t i = 2048000 / BAUD_RATE; i > 0; i--) {
				_delay_us(250);
			}
			TCB0.CTRLA |= TCB_ENABLE_bm;
		}
	}
}

ISR(TCB0_INT_vect)
{
/* Initialize variables */
#ifdef ENCODING_G_E_THOMAS
	static uint8_t prev = 0;
#else
	static uint8_t prev = 1;
#endif

	static uint8_t clk                   = 1;
	static uint8_t transmit_buffer_index = 0;

	/*
	 * The most significant bit is sent first. We therefore start at
	 * bit_index = 7, and work our way down to bit_index = 0. However,
	 * we have already accounted for the first bit in the packet by
	 * setting PORTA.OUTSET in main, and therefore start at bit_index = 6.
	 */
	static uint8_t bit_index = 6;

	/* Encode signal and output to PA4 */
	if (clk) {
		/* Always toggle pin on clock edge */
		clk          = 0;
		PORTA.OUTTGL = PIN4_bm;
	} else {
		uint8_t next_bit = (transmit_buffer[transmit_buffer_index] & (1 << bit_index)) > 0;

		if (next_bit == prev) {
			PORTA.OUTTGL = PIN4_bm;
		}

		/* Handle iteration variables and avoid overflow */
		if (!bit_index--) {
			bit_index = 7;

			if (++transmit_buffer_index >= transmit_buffer_length) {
/* Prepare for next packet */
#ifdef ENCODING_G_E_THOMAS
				PORTA.OUTCLR = PIN4_bm;
#else
				PORTA.OUTSET = PIN4_bm;
#endif

				transmit_buffer_index = 0;
				bit_index             = 6;

				/* Disable TCB after the packet is sent */
				TCB0.CTRLA          = (TCB0.CTRLA & ~TCB_ENABLE_bm);
				sending_in_progress = 0;
			}
		}

		clk  = 1;
		prev = next_bit;
	}

	/* Clear TCB0 interrupt flag */
	TCB0.INTFLAGS = TCB_CAPT_bm;
}
