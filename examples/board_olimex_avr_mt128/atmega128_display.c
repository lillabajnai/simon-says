// NOTE: lines below added to allow compilation in simavr's build system
#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

/*	Sample program for Olimex AVR-MT-128 with ATMega-128 processor
 *	Button 1 (Up)		- Turn ON the display
 *	Button 2 (Left)		- Set Text1 on the upline
 *	Button 3 (Middle)	- Hold to slide Text
 *	Button 4 (Right)	- Set Text2 on the downline
 *	Button 5 (Down)		- Turn OFF the display
 *	Compile with AVRStudio+WinAVR (gcc version 3.4.6)
 */

 #define Text1	"Conflux"
 #define Text2	"Rampard"



#include "avr/io.h"
#include "avr_lcd.h" // NOTE: changed header name to better fit in simavr's file naming conventions

#define	__AVR_ATMEGA128__	1

unsigned char data, Line = 0;
char Text[16], Ch;
unsigned int Bl = 1, LCD_State = 0, i, j;

void Delay(unsigned int b)
{
  volatile unsigned int a = b;  // NOTE: volatile added to prevent the compiler to optimization the loop away
  while (a)
	{
		a--;
	}
}

/*****************************L C D**************************/

void E_Pulse()
{
	PORTC = PORTC | 0b00000100;	//set E to high
	Delay(1400); 				//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

void LCD_Init()
{
	//LCD initialization
	//step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	Delay(10000);


	PORTC = 0b00110000;						//set D4, D5 port to 1
	E_Pulse();								//high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000;						//set D4, D5 port to 1
	E_Pulse();								//high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00110000;						//set D4, D5 port to 1
	E_Pulse();								//high->low to E port (pulse)
	Delay(1000);

	PORTC = 0b00100000;						//set D4 to 0, D5 port to 1
	E_Pulse();								//high->low to E port (pulse)
}

void LCDSendCommand(unsigned char a)
{
	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	PORTC = PORTC & 0b11111110;				//set RS port to 0
	E_Pulse();                              //pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	PORTC = PORTC & 0b11111110;				//set RS port to 0 -> display set to command mode
	E_Pulse();                              //pulse to set d4-d7 bits

}

void LCDSendChar(unsigned char a)
{
	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	PORTC = PORTC | 0b00000001;				//set RS port to 1
	E_Pulse();                              //pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//clear D4-D7
	PORTC = PORTC | 0b00000001;				//set RS port to 1 -> display set to command mode
	E_Pulse();                              //pulse to set d4-d7 bits
}

void LCDSendTxt(char* a)
{
	int Temp;
	for(Temp=0; Temp<strlen(a); Temp++)
  {
    LCDSendChar(a[Temp]);
  }
}

void LCDSendInt(long a)
{
	int C[20];
	unsigned char Temp=0, NumLen = 0;
	if (a < 0)
	{
		LCDSendChar('-');
		a = -a;
	}
	do
	{
		Temp++;
		C[Temp] = a % 10;
		a = a/10;
	}
	while (a);
	NumLen = Temp;
	for (Temp = NumLen; Temp>0; Temp--) LCDSendChar(C[Temp] + 48);
}

void LCDSendInt_Old(int a)
{
  int h = 0;
  int l = 0;

  l = a%10;
  h = a/10;

  LCDSendChar(h+48);
  LCDSendChar(l+48);
}

void SmartUp(void)
{
	int Temp;
	for(Temp=0; Temp<1; Temp++) LCDSendCommand(CUR_UP);
}

void SmartDown(void)
{
	int Temp;
	for(Temp=0; Temp<40; Temp++) LCDSendCommand(CUR_DOWN);
}

void Light(short a)
{
  if(a == 1)
  {
	PORTC = PORTC | 0b00100000;
	DDRC = PORTC | 0b00100000;

    //IO0SET_bit.P0_25 = 1;
    //IO0DIR_bit.P0_25 = 1;
  }
  if(a == 0)
  {
    PORTC = PORTC & 0b11011111;
    DDRC = DDRC & 0b11011111;

    //IO0SET_bit.P0_25 = 0;
    //IO0DIR_bit.P0_25 = 0;
  }

}

/*****************************L C D**************************/

void Port_Init()
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
	Port_Init();
	LCD_Init();
	// NOTE: added missing initialization steps
	LCDSendCommand(0x28); // function set: 4 bits interface, 2 display lines, 5x8 font
	LCDSendCommand(DISP_OFF); // display off, cursor off, blinking off
	LCDSendCommand(CLR_DISP); // clear display
	LCDSendCommand(0x06); // entry mode set: cursor increments, display does not shift

	LCDSendCommand(DISP_OFF);
	while (1)
	{
		//Value of Bl prevents holding the buttons
		//Bl = 0: One of the Buttons is pressed, release to press again
		//LCD_State value is the state of LCD: 1 - LCD is ON; 0 - LCD is OFF

		//Up Button (Button 1) : Turn ON Display
 		if (!(PINA & 0b00000001) & Bl & !(LCD_State))	//check state of button 1 and value of Bl and LCD_State
		{
			LCDSendCommand(DISP_ON);		//Turn ON Display
			LCDSendCommand(CLR_DISP);		//Clear Display
			LCDSendTxt("    Welcome     ");	//Print welcome screen
			Bl = 0;							//Button is pressed
			LCD_State = 1;					//Display is ON (LCD_State = 1)
		}

		//Left Button (Button 2) : Set Text1
		if (!(PINA & 0b00000010) & Bl & LCD_State)		//check state of button 2 and value of Bl and LCD_State
		{
			LCDSendCommand(CLR_DISP);			//Clear Display
			LCDSendCommand(DD_RAM_ADDR);		//set to Line 1
			for (i=0; i<16; i++) Text[i] = 0;	//Clear Text
			strcpy(Text, Text1);				//set Text
			for (i=0; i<16; i++)
				if (Text[i] == 0) Text[i] = ' ';//fill the Text with space
			//LCDSendTxt(Text);					//Print the text
			for (i=0; i<16; i++) LCDSendChar(Text[i]);
			Bl = 0;								//Button is pressed
			Line = 1;							//text must be print on up line
		}

		//Middle Button (Button 3) : Slide the text
		if (!(PINA & 0b00000100) & Bl & LCD_State)
		{
			while (!(PINA & 0b00000100) & (Line != 0))
			//check state of button 3 and value of Line (Line = 0 - no text selected)
			{
				//sliding the text
				for(i=60000; i; i--);
				Ch = Text[0];
				for (i=0; i<15; i++) Text[i] = Text[i+1];
				Text[15] = Ch;

				//print the text in the correct line
				if (Line == 1)
					LCDSendCommand(DD_RAM_ADDR);
				else
					LCDSendCommand(DD_RAM_ADDR2);
				//LCDSendTxt(Text);
				for (i=0; i<16; i++) LCDSendChar(Text[i]);
			}
		}

		//Right Button (Button 4) Set Text2
		if (!(PINA & 0b00001000) & Bl & LCD_State)	//check state of button 4 and value of Bl and LCD_State
		{
			LCDSendCommand(CLR_DISP);			//Clear Display
			LCDSendCommand(DD_RAM_ADDR2);		//set to Line 2
			for (i=0; i<16; i++) Text[i] = 0;	//Clear Text
			strcpy(Text, Text2);				//set Text
			for (i=0; i<16; i++)
				if (Text[i] == 0) Text[i] = ' ';//fill the Text with space
			//LCDSendTxt(Text);					//Print the text
			for (i=0; i<16; i++) LCDSendChar(Text[i]);
			Bl = 0;								//Button is pressed
			Line = 2;							//text must be print on down line
		}

		//Down Button (Button 5) Turn OFF Display
		if (!(PINA & 0b00010000) & Bl & LCD_State)	//check state of button 5 and value of Bl and LCD_State
		{
			LCDSendCommand(CLR_DISP);			//Clear Display
			LCDSendTxt("    Turn OFF    ");		//Print Turn OFF message
			for (i=30; i; i--)
			for (j=30000; j; j--);				//simple delay
			Bl = 0;								//Button is pressed
			LCDSendCommand(DISP_OFF);			//Display is OFF ==> LCD_State = 0, Line = 0
			LCD_State = 0;
			Line = 0;
		}

		//check state of all buttons
		if (
			((PINA & 0b00000001)
			|(PINA & 0b00000010)
			|(PINA & 0b00000100)
			|(PINA & 0b00001000)
			|(PINA & 0b00010000)) == 31)
		Bl = 1;		//if all buttons are released Bl gets value 1


	}
	return 0;
}
