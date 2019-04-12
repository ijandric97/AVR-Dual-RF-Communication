#define F_CPU 7372800UL // #TODO -> dodat pod project properties -> toolchain -> c++ symbols -> defined symbols

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#include "lcd.h"
#include "VirtualWire.h"

// #CONSTANTS# ===================================================================================================================
#define VW_PLATFORM VW_PLATFORM_GENERIC_AVR8		  // Select AVR8 platform
#define vw_timerSetup(speed) my_vw_timerSetup(speed); // Instruct VirtualWire to use my timer setup routine and my interrupt vector
#define VW_TIMER_VECTOR TIMER1_COMPB_vect

#define LED_BLINK 4	// Ledica koja blinka kod transmitanja valjda
#define VW_BPS 100	// Bits per second -> U proteusu nema nikakvog efekta, uvijek treba minuta da se posalje
#define MSG_LEN 33
#define MSG_BLANK "                                "
#define TRANSMIT_COUNT 2	// Broj puta koji transmitamo, 1 je dovoljno al za svaki slucaj

#define KEY_UP 0
#define KEY_DOWN 1
#define KEY_NEXT 2
#define KEY_SEND 3

#define BNC_DELAY_FAST 20
#define BNC_DELAY_SLOW 200
// ===============================================================================================================================

// #VARIABLES# ===================================================================================================================
char Alphabet[27] = {' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
char Message[MSG_LEN] = MSG_BLANK;
uint8_t MessageIndex = 0;
uint8_t CharacterIndex = 0;
// ===============================================================================================================================

// #FUNCTIONS# ===================================================================================================================
uint8_t vw_timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks);
static inline void my_vw_timerSetup(uint8_t speed) __attribute__ ((always_inline)); // Declare my own timer setup function
inline void Transmit();
inline void Input();
inline void Display();
inline void Setup();
// ===============================================================================================================================

uint8_t vw_timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks) { // #TODO
	*nticks = 3685;		// VirtualWire has a special routine for detecting prescaler and the number of ticks
	return (1 << CS10); // automatically, but needs to be declared first in order to be used
}

static inline void my_vw_timerSetup(uint8_t speed) {
	// Define my setup timer routine, that uses OCR1B as compare register
	// Figure out prescaler value and counter match value
	uint16_t nticks;
	uint8_t prescaler = vw_timer_calc(speed, (uint16_t)-1, &nticks);
	if (!prescaler) {
		return;
	}

	TCCR1A = 0;				// Output Compare pins disconnected
	TCCR1B = (1 << WGM12);	// Turn on CTC mode
	TCCR1B |= prescaler;	// Convert prescaler index to TCCR1B prescaler bits CS10, CS11, CS12
	OCR1B = nticks;			// Caution: special procedures for setting 16 bit regs is handled by the compiler
	TIMSK |= (1 << OCIE1B);	// Enable interrupt
}

inline void Transmit() {
	PORTA ^= (1 << LED_BLINK); 	// #TODO -> ovo mi se cini da nije potrebno jer virtualwire sam po sebi flasha kao blesav dvije vanjske ledice :)
	vw_send((uint8_t *)Message, strlen(Message));
	vw_wait_tx();
	PORTA ^= (1 << LED_BLINK);
}

inline void Input() {
	if (bit_is_clear(PINB, KEY_UP)) {
		while (bit_is_clear(PINB, KEY_UP)) {
			_delay_ms(10);	// Cekamo dok ne pustimo tipku
		}
		
		CharacterIndex++;
		if (CharacterIndex > 26) {
			CharacterIndex = 0;		// Ako smo izasli izvan alfabeta, vracamo se na prvi element, koji je SPACE
		}
		Message[MessageIndex] = Alphabet[CharacterIndex];	// Dodamo taj znak u nasu poruku
		_delay_ms(BNC_DELAY_FAST);	// #TODO nezz dal su ovi debouncevi uopce potrebni, u proteusu sigurno nisu, al real shit nezz
	}
	if (bit_is_clear(PINB, KEY_DOWN)) {
		while (bit_is_clear(PINB, KEY_DOWN)) {
			_delay_ms(10);	// Cekamo dok ne pustimo tipki
		}
		
		if (CharacterIndex == 0) {
			CharacterIndex = 26;
		} else {
			CharacterIndex--;
		}
		Message[MessageIndex] = Alphabet[CharacterIndex];
		_delay_ms(BNC_DELAY_FAST);	// #TODO
	}
	if (bit_is_clear(PINB, KEY_NEXT)) {
		while (bit_is_clear(PINB, KEY_NEXT)) {
			_delay_ms(10);	// Cekamo dok ne pustimo tipki
		}
		
		MessageIndex++;
		Message[MessageIndex] = Alphabet[0];
		CharacterIndex = 0;
		if (MessageIndex > 31) {
			MessageIndex = 0;
		}
		_delay_ms(BNC_DELAY_SLOW);	// #TODO
	}
	if (bit_is_clear(PINB, KEY_SEND)) {
		while (bit_is_clear(PINB, KEY_SEND)) {
			_delay_ms(10);	// Cekamo dok ne pustimo tipki
		}
		
		for (uint8_t trc = 1; trc <= TRANSMIT_COUNT; trc++) {
			lcd_clrscr();
			char buffer[2];
			itoa(trc, buffer, 10);
			lcd_puts("Transmitting ");			// Napisi da saljemo
			lcd_puts(buffer);
			Transmit();							// Posalji poruku
			_delay_ms(50);
		}
		MessageIndex = 0;						// Resetiraj indekse
		CharacterIndex = 0;
		memcpy(Message, MSG_BLANK, MSG_LEN);	// Resetiraj poruku
	}
}

inline void Display() {
	char FirstLine[17];
	char SecondLine[17];
	
	lcd_clrscr();	
	
	memcpy(FirstLine, &Message[0], 16);		// Podijeli poruku na prvih 16 znakova (gornji red)
	FirstLine[16] = '\0';
	memcpy(SecondLine, &Message[16], 16);	// i drugih 16 znakova (donji red)
	SecondLine[16] = '\0';
	
	lcd_gotoxy(0, 0);		// Ispis prvog reda
	lcd_puts(FirstLine);
	lcd_gotoxy(0, 1);		// Ispis drugog reda
	lcd_puts(SecondLine);
}

inline void Setup() {
	// Postavi LED
	DDRA  = (1 << LED_BLINK);
	PORTA = (1 << LED_BLINK);
	
	// Postavi BTN0-BTN3
	PORTB = 0x0F;
	DDRB  = 0xF0;
	
	//Postavi LCD
	DDRD = (1 << 4);
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
	lcd_puts("Transmitter");
	
	// Initialise the IO and ISR
	vw_set_ptt_inverted(1);
	vw_setup(VW_BPS);	 // Bits per sec
	vw_rx_start();       // Start the receiver PLL running
	sei();
}

int main() {
	Setup();
	_delay_ms(1000); // Cisto da se vidi ona prva poruka
	
	while (1) {
		Display();
		Input();
	}
}