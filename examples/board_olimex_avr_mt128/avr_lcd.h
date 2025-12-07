//lcd.h
#include <avr/io.h>

//#define		DISP_ON		0x0000000C	//LCD on
//#define		DISP_OFF	0x00000008	//LCD off
//#define		CLR_DISP	0x00000001	//LCD clear
//#define		CUR_HOME	0x00000002	//LCD cursor home
//#define		ENTRY_INC	0x00000007	//LCD increment
//#define		ENTRY_DEC	0x00000005	//LCD decrement
//#define		DD_RAM_ADDR	0x00000080	//LCD 1 row
//#define		DD_RAM_ADDR2	0x000000C0	//LCD 2 row
//#define		SH_LCD_LEFT	0x00000010	//LCD shift left
//#define		SH_LCD_RIGHT	0x00000014	//LCD shift right
//#define		MV_LCD_LEFT	0x00000018	//LCD move left
//#define		MV_LCD_RIGHT	0x0000001C	//LCD move right

#define		CLR_DISP	0x00000001
#define		DISP_ON		0x0000000C
#define		DISP_OFF	0x00000008
#define		CUR_HOME        0x00000002
#define		CUR_OFF 	0x0000000C
#define         CUR_ON_UNDER    0x0000000E
#define         CUR_ON_BLINK    0x0000000F
#define         CUR_LEFT        0x00000010
#define         CUR_RIGHT       0x00000014
#define		CUR_UP  	0x00000080
#define		CUR_DOWN	0x000000C0
#define         ENTER           0x000000C0
#define		DD_RAM_ADDR	0x00000080
#define		DD_RAM_ADDR2	0x000000C0


void E_Pulse();
void LCDInit();
void LCDSendCommand(unsigned char a);
void LCDSendChar(unsigned char a);
void LCDSendTxt(char* a);
void SmartUp(void);
void SmartDown(void);
void Light(short a);
void LCDSendInt(long a);
