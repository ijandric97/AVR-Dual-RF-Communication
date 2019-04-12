#define F_CPU 8000000UL // #TODO -> dodat pod project properties -> toolchain -> c++ symbols -> defined symbols

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#include "lcd.h"
#include "VirtualWire.h"

// #CONSTANTS# ===================================================================================================================
#define VW_PLATFORM VW_PLATFORM_GENERIC_AVR8 // Select AVR8 platform
#define vw_timerSetup(speed) my_vw_timerSetup(speed); // Instruct VirtualWire to use my timer setup routine and my interrupt vector
#define VW_TIMER_VECTOR TIMER1_COMPB_vect

#define LED_BLINK 4	// Ledica koja blinka kod transmitanja valjda
#define VW_BPS 100	// Bits per second -> U proteusu nema nikakvog efekta, uvijek treba minuta da se posalje
#define MSG_LEN 33
#define MSG_BLANK "                                "
// ===============================================================================================================================

// #VARIABLES# ===================================================================================================================
uint8_t Message[MSG_LEN] =	MSG_BLANK;
uint8_t MessageLength = MSG_LEN;
uint16_t GoodMessage = 0;
// ===============================================================================================================================

// #FUNCTIONS# ===================================================================================================================
uint8_t vw_timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks);
static inline void my_vw_timerSetup(uint8_t speed) __attribute__ ((always_inline)); // Declare my own timer setup function
inline void Receive();
inline void Display();
inline void Setup();
// ===============================================================================================================================

uint8_t vw_timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks) { // #TODO -> nezz pitaj paula
	*nticks = 3999;		// VirtualWire has a special routine for detecting prescaler and the number of ticks
	return (1 << CS10);	// automatically, but needs to be declared first in order to be used
}

static inline void my_vw_timerSetup(uint8_t speed) {
	// Define my setup timer routine, that uses OCR1B as compare register
	// Figure out prescaler value and counter match value
	uint16_t nticks;
	uint8_t prescaler = vw_timer_calc(speed, (uint16_t)-1, &nticks);
 	if (!prescaler) {
 		return; // fault
 	}

	TCCR1A = 0;				// Output Compare pins disconnected
	TCCR1B = (1 << WGM12);	// Turn on CTC mode
	TCCR1B |= prescaler;	// Convert prescaler index to TCCR1B prescaler bits CS10, CS11, CS12
	OCR1B = nticks;			// Caution: special procedures for setting 16 bit regs is handled by the compiler
	TIMSK |= (1 << OCIE1B); // Enable interrupt
}

inline void Receive() {
	PORTA ^= (1 << LED_BLINK);
	
	memcpy(Message, MSG_BLANK, MSG_LEN);
	MessageLength = MSG_LEN;
	
    if (vw_get_message((uint8_t *)Message, &MessageLength)) {
		GoodMessage = 1;
    } else {
		GoodMessage = 0;
	}

    PORTA ^= (1 << LED_BLINK);
}

inline void Display() {
	char FirstLine[17];
	char SecondLine[17];

	lcd_clrscr();
	
	memcpy(FirstLine, &Message[0], 16);
	FirstLine[16] = '\0';
	memcpy(SecondLine, &Message[16], 16);
	SecondLine[16] = '\0';
	
	lcd_gotoxy(0, 0);
	lcd_puts(FirstLine);
	lcd_gotoxy(0, 1);
	lcd_puts(SecondLine);
}

inline void Setup() {
	// Postavi LED
	DDRA  = (1 << LED_BLINK);
	PORTA = (1 << LED_BLINK);

	// Postavi LCD
	DDRD = (1 << 4);
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
	lcd_puts("Receiver");
	
	// Postavi receiver (IO i ISR)
	vw_setup(VW_BPS);	// Bits per sec
	vw_rx_start();		// Start the receiver PLL running
	sei();
}

int main() {	
	Setup();
	_delay_ms(1000); // Cisto da se vidi ona prva poruka
	
	while(1) {
		if (GoodMessage != 0) {
			Display();
		}
		Receive();
	}
}
