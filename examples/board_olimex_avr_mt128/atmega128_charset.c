// NOTE: lines below added to allow compilation in simavr's build system
#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#include "avr/io.h"
#include "avr_lcd.h"  // NOTE: changed header name to better fit in simavr's file naming conventions

#define	__AVR_ATMEGA128__	1

static void Delay(unsigned int b)
{
  volatile unsigned int a = b;  // NOTE: volatile added to prevent the compiler to optimization the loop away
  while (a)
	{
		a--;
	}
}

static void EPulse()
{
	PORTC = PORTC | 0b00000100;  // set E to high
	Delay(1400);                 // delay ~110ms
	PORTC = PORTC & 0b11111011;  // set E to low
}

void LCDInit()
{
	// LCD initialization
	// step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	Delay(10000);


	PORTC = 0b00110000;        // set D4, D5 port to 1
	EPulse();                  // high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000;        // set D4, D5 port to 1
	EPulse();                  // high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000;        // set D4, D5 port to 1
	EPulse();                  // high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00100000;        // set D4 to 0, D5 port to 1
	EPulse();                  // high->low to E port (pulse)

  // NOTE: added missing initialization steps
	LCDSendCommand(0x28);      // function set: 4 bits interface, 2 display lines, 5x8 font
	LCDSendCommand(DISP_OFF);  // display off, cursor off, blinking off
	LCDSendCommand(CLR_DISP);  // clear display
	LCDSendCommand(0x06);      // entry mode set: cursor increments, display does not shift
}

void LCDSendCommand(unsigned char a)
{
	unsigned char data = 0b00001111 | a;   // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;   // set D4-D7
	PORTC = PORTC & 0b11111110;            // set RS port to 0
	EPulse();                              // pulse to set D4-D7 bits

	data = a<<4;                           // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;   // set D4-D7
	PORTC = PORTC & 0b11111110;            // set RS port to 0 -> display set to command mode
	EPulse();                              // pulse to set d4-d7 bits
}

void LCDSendChar(unsigned char a)
{
	unsigned char data = 0b00001111 | a;   // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;   // set D4-D7
	PORTC = PORTC | 0b00000001;            // set RS port to 1
	EPulse();                              // pulse to set D4-D7 bits

	data = a<<4;                           // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;   // clear D4-D7
	PORTC = PORTC | 0b00000001;            // set RS port to 1 -> display set to command mode
	EPulse();                              // pulse to set d4-d7 bits
}

static unsigned char cgram[64] = {
  0b00000, 0b01110, 0b01010, 0b01010, 0b01010, 0b01110, 0b00000, 0b11111,
  0b00000, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b11111,
  0b00000, 0b01110, 0b00010, 0b01110, 0b01000, 0b01110, 0b00000, 0b11111,
  0b00000, 0b01110, 0b00010, 0b01110, 0b00010, 0b01110, 0b00000, 0b11111,
  0b00000, 0b01000, 0b01000, 0b01110, 0b00100, 0b00100, 0b00000, 0b11111,
  0b00000, 0b01110, 0b01000, 0b01110, 0b00010, 0b01110, 0b00000, 0b11111,
  0b00000, 0b01110, 0b01000, 0b01110, 0b01010, 0b01110, 0b00000, 0b11111,
  0b00000, 0b01110, 0b00010, 0b00010, 0b00010, 0b00010, 0b00000, 0b11111,
};

static void LCDInitCGRAM()
{
  LCDSendCommand(0x40);
  for (unsigned int i = 0; i < 64; ++i) {
    LCDSendChar(cgram[i]);
  }
}

static void PortInit()
{
	PORTA = 0b00011111;		DDRA = 0b01000000; // NOTE: set A4-0 to initialize buttons to unpressed state
	PORTB = 0b00000000;		DDRB = 0b00000000;
	PORTC = 0b00000000;		DDRC = 0b11110111;
	PORTD = 0b11000000;		DDRD = 0b00001000;
	PORTE = 0b00000000;		DDRE = 0b00110000;
	PORTF = 0b00000000;		DDRF = 0b00000000;
	PORTG = 0b00000000;		DDRG = 0b00000000;
}

int main()
{
	PortInit();
	LCDInit();
  LCDInitCGRAM();

  unsigned int b = 1; // button lock (0: pressed, 1: released)
  unsigned int c = 0; // char start index

  while (1)
	{
    // Middle Button (Button 3)
		if (!(PINA & 0b00000100) && b)
		{
      LCDSendCommand(DD_RAM_ADDR);
      for (unsigned int i = 0; i < 16; ++i)
      {
        LCDSendChar(c + i);
      }
      LCDSendCommand(DD_RAM_ADDR2);
      for (unsigned int i = 0; i < 16; ++i)
      {
        LCDSendChar(c + 16 + i);
      }
      c = (c + 32) & 0xff;
      b = 0;  // button is pressed
		}

    //check state of all buttons
    if (
      ((PINA & 0b00000001)
      |(PINA & 0b00000010)
      |(PINA & 0b00000100)
      |(PINA & 0b00001000)
      |(PINA & 0b00010000)) == 31)
    b = 1;  // if all buttons are released b gets value 1
  }

	return 0;
}
