/**
 * Simon says
 * by Lilla Bajnai
 */

#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define	__AVR_ATmega128__	1
#include <avr/io.h>

// CONFIGURATION -------------------------------------------------------------

// Set to 1 to enable input timeout (for gameplay)
// Set to 0 to disable timeout (for testing/development - infinite time to press buttons)
#define ENABLE_TIMEOUT 1

// Maximum level in classic mode
#define MAX_LEVEL_CLASSIC 20

// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init() {
	PORTA = 0b00011111;	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;	DDRB = 0b00000000;
	PORTC = 0b00000000;	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;	DDRD = 0b00001000;
	PORTE = 0b00100000;	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;	DDRF = 0b00000000;
	PORTG = 0b00000000;	DDRG = 0b00000000;
}

// TIMER-BASED RANDOM NUMBER GENERATOR ---------------------------------------

static void rnd_init() {
	TCCR0 |= (1  << CS00);	// Timer 0 no prescaling (@FCPU)
	TCNT0 = 0; 				// init counter
}

// generate a value between 0 and max
static int rnd_gen(int max) {
	return TCNT0 % max;
}

// SOUND GENERATOR -----------------------------------------------------------

typedef struct {
	int freq;
	int length;
} tune_t;

static tune_t TUNE_START[] = { { 2000, 40 }, { 0, 0 } };
static tune_t TUNE_LEVELUP[] = { { 3000, 20 }, { 0, 0 } };
static tune_t TUNE_GAMEOVER[] = { { 1000, 200 }, { 1500, 200 }, { 2000, 400 }, { 0, 0 } };

static void play_note(int freq, int len) {
	for (int l = 0; l < len; ++l) {
		int i;
		PORTE = (PORTE & 0b11011111) | 0b00010000;	//set bit4 = 1; set bit5 = 0
		for (i=freq; i; i--);
		PORTE = (PORTE | 0b00100000) & 0b11101111;	//set bit4 = 0; set bit5 = 1
		for (i=freq; i; i--);
	}
}

static void play_tune(tune_t *tune) {
	while (tune->freq != 0) {
		play_note(tune->freq, tune->length);
		++tune;
	}
}

// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE		0
#define BUTTON_CENTER	1
#define BUTTON_LEFT		2
#define BUTTON_RIGHT	3
#define BUTTON_UP		4
#define BUTTON_DOWN		5
static int button_accept = 1;

static int button_pressed() {
	// right
	if (!(PINA & 0b00000001) && button_accept) {
		button_accept = 0;
		return BUTTON_RIGHT;
	}

	// up
	if (!(PINA & 0b00000010) && button_accept) {
		button_accept = 0;
		return BUTTON_UP;
	}

	// center
	if (!(PINA & 0b00000100) && button_accept) {
		button_accept = 0;
		return BUTTON_CENTER;
	}

	// left
	if (!(PINA & 0b00010000) && button_accept) {
		button_accept = 0;
		return BUTTON_LEFT;
	}

	// down
	if (!(PINA & 0b00001000) && button_accept) {
		button_accept = 0;
		return BUTTON_DOWN;
	}

	return BUTTON_NONE;
}

static void button_unlock() {
	if (
		((PINA & 0b00000001)
		|(PINA & 0b00000010)
		|(PINA & 0b00000100)
		|(PINA & 0b00001000)
		|(PINA & 0b00010000)) == 31)
	button_accept = 1;
}

// LCD HELPERS ---------------------------------------------------------------

#define		CLR_DISP	    0x00000001
#define		DISP_ON		    0x0000000C
#define		DISP_OFF	    0x00000008
#define		CUR_HOME      0x00000002
#define		CUR_OFF 	    0x0000000C
#define   CUR_ON_UNDER  0x0000000E
#define   CUR_ON_BLINK  0x0000000F
#define   CUR_LEFT      0x00000010
#define   CUR_RIGHT     0x00000014
#define   CG_RAM_ADDR		0x00000040
#define		DD_RAM_ADDR	  0x00000080
#define		DD_RAM_ADDR2	0x000000C0

//#define		ENTRY_INC	    0x00000007	//LCD increment
//#define		ENTRY_DEC	    0x00000005	//LCD decrement
//#define		SH_LCD_LEFT	  0x00000010	//LCD shift left
//#define		SH_LCD_RIGHT	0x00000014	//LCD shift right
//#define		MV_LCD_LEFT	  0x00000018	//LCD move left
//#define		MV_LCD_RIGHT	0x0000001C	//LCD move right

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1400); 			//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

static void lcd_send(int command, unsigned char a) {
	unsigned char data;

	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a) {
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a) {
	lcd_send(0, a);
}

static void lcd_init() {
	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000;				//set D4 to 0, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)

	lcd_send_command(0x28);
	lcd_send_command(DISP_OFF);
	lcd_send_command(CLR_DISP);
	lcd_send_command(0x06);

	lcd_send_command(DISP_ON);
	lcd_send_command(CLR_DISP);
}

static void lcd_send_text(char *str) {
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str) {
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str) {
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

// MENU SYSTEM ---------------------------------------------------------------

#define MENU_CLASSIC    0
#define MENU_HARDCORE   1
#define MENU_HIGHSCORES 2
#define MENU_NUM        3

// Simple arrow character for menu
static unsigned char MENU_ARROW[8] = {
	0b00000,
	0b01000,
	0b01100,
	0b01110,
	0b01100,
	0b01000,
	0b00000,
	0b00000
};

static void menu_init_custom_chars() {
	lcd_send_command(CG_RAM_ADDR);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(MENU_ARROW[i]);
}

static void menu_display(int selected) {
	lcd_send_command(CLR_DISP);

	for (volatile int i = 0; i < 1000; ++i);

	lcd_send_command(DD_RAM_ADDR);

	if (selected == MENU_CLASSIC) {
		lcd_send_data(0);
		lcd_send_text(" CLASSIC MODE");
	} else if (selected == MENU_HARDCORE) {
		lcd_send_data(0);
		lcd_send_text(" HARDCORE MODE");
	} else if (selected == MENU_HIGHSCORES) {
		lcd_send_data(0);
		lcd_send_text(" HIGH SCORES");
	}

	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text("E/F NAV  ENTER");
}

static int show_menu() {
	int selected = 0;
	int button;

	menu_init_custom_chars();
	menu_display(selected);

	while (1) {
		button = button_pressed();

		if (button == BUTTON_UP) {
			selected = (selected - 1 + MENU_NUM) % MENU_NUM;
			menu_display(selected);
			play_note(2000, 10);
		} else if (button == BUTTON_LEFT) {
			selected = (selected + 1) % MENU_NUM;
			menu_display(selected);
			play_note(2000, 10);
		} else if (button == BUTTON_DOWN) {
			play_note(3000, 30);
			return selected;
		}

		button_unlock();

		for (volatile int i = 0; i < 5000; ++i);
	}
}

// CLASSIC GAME MODE ---------------------------------------------------------

#define MAX_SEQUENCE 100
static unsigned char sequence[MAX_SEQUENCE];
static int sequence_length = 0;
static int current_level = 1;
static int lives = 3;
static int score = 0;
static int best_hardcore = 0;
static int best_classic_level = 0;
static int best_classic_score = 0;

// Button to color mapping for Simon
#define COLOR_RED    BUTTON_UP
#define COLOR_GREEN  BUTTON_RIGHT
#define COLOR_BLUE   BUTTON_LEFT
#define COLOR_YELLOW BUTTON_CENTER

// Character 0: Left bracket [ (empty)
static unsigned char BRACKET_LEFT_EMPTY[8] = {
	0b11111,
	0b11000,
	0b11000,
	0b11000,
	0b11000,
	0b11000,
	0b11000,
	0b11111
};

// Character 2: Left bracket [ (lit/grained)
static unsigned char BRACKET_LEFT_LIT[8] = {
	0b11111,
	0b11101,
	0b11010,
	0b11101,
	0b11010,
	0b11101,
	0b11010,
	0b11111
};

// Helper function to flip a character pattern horizontally (creates ] from [)
static void flip_char_horizontal(unsigned char *src, unsigned char *dst) {
	for (int i = 0; i < 8; ++i) {
		unsigned char b = src[i];
		dst[i] = ((b & 0b10000) >> 4) |
		         ((b & 0b01000) >> 2) |
		         ((b & 0b00100)     ) |
		         ((b & 0b00010) << 2) |
		         ((b & 0b00001) << 4);
	}
}

// Character 4: Heart
static unsigned char HEART_CHAR[8] = {
	0b00000,
	0b01010,
	0b11111,
	0b11111,
	0b11111,
	0b01110,
	0b00100,
	0b00000
};

// Heart breaking animation frames
static unsigned char HEART_BREAK_FRAMES[5][8] = {
	// Frame 0: Full heart
	{
		0b00000,
		0b01010,
		0b11111,
		0b11111,
		0b11111,
		0b01110,
		0b00100,
		0b00000
	},
	// Frame 1: Cracking
	{
		0b00000,
		0b01010,
		0b11011,
		0b11011,
		0b11111,
		0b01110,
		0b00100,
		0b00000
	},
	// Frame 2: Breaking apart
	{
		0b00000,
		0b01010,
		0b10101,
		0b10001,
		0b10001,
		0b01010,
		0b00100,
		0b00000
	},
	// Frame 3: Fragments
	{
		0b00000,
		0b01010,
		0b00100,
		0b00000,
		0b00000,
		0b00100,
		0b00000,
		0b00000
	},
	// Frame 4: Completely empty (heart fully disappeared)
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000
	}
};

// Character 5: Skull (for hardcore mode)
static unsigned char SKULL_CHAR[8] = {
	0b01110,
	0b10101,
	0b11011,
	0b01110,
	0b01110,
	0b01010,
	0b01110,
	0b00000
};

// Character 6: Progress bar filled
static unsigned char PROGRESS_FULL[8] = {
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111
};

// Horizontal timer bar character
static unsigned char TIMER_HORIZONTAL[6][8] = {
	// Frame 0: Empty (0 columns filled)
	{0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
	// Frame 1: 1 column filled (leftmost - 20%)
	{0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000},
	// Frame 2: 2 columns filled (40%)
	{0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000},
	// Frame 3: 3 columns filled (60%)
	{0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100},
	// Frame 4: 4 columns filled (80%)
	{0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110},
	// Frame 5: Full (5 columns filled - 100%)
	{0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111}
};

static void init_game_chars() {
	unsigned char flipped[8];

	lcd_send_command(CG_RAM_ADDR);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(BRACKET_LEFT_EMPTY[i]);

	flip_char_horizontal(BRACKET_LEFT_EMPTY, flipped);
	lcd_send_command(CG_RAM_ADDR + 8);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(flipped[i]);

	lcd_send_command(CG_RAM_ADDR + 16);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(BRACKET_LEFT_LIT[i]);

	flip_char_horizontal(BRACKET_LEFT_LIT, flipped);
	lcd_send_command(CG_RAM_ADDR + 24);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(flipped[i]);

	lcd_send_command(CG_RAM_ADDR + 32);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(HEART_CHAR[i]);

	lcd_send_command(CG_RAM_ADDR + 40);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(SKULL_CHAR[i]);

	lcd_send_command(CG_RAM_ADDR + 48);
	for (int i = 0; i < 8; ++i)
		lcd_send_data(PROGRESS_FULL[i]);
}

static void animate_heart_break(int old_lives, int new_lives) {
	for (int frame = 0; frame < 5; ++frame) {
		lcd_send_command(CG_RAM_ADDR + 56);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(HEART_BREAK_FRAMES[frame][i]);

		lcd_send_command(DD_RAM_ADDR2 + 9);
		for (int j = 0; j < new_lives; ++j)
			lcd_send_data(4);
		lcd_send_data(7);
		for (int j = new_lives + 1; j < old_lives; ++j)
			lcd_send_text(" ");

		for (volatile int p = 0; p < 8000; ++p)
			for (volatile int q = 0; q < 30; ++q);
	}
}

static void draw_board_classic(int highlight) {
	lcd_send_command(DD_RAM_ADDR);

	// Line 1: game grid on left, lives + level on right
	lcd_send_data(highlight == COLOR_RED ? 2 : 0);
	lcd_send_data(highlight == COLOR_RED ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_data(highlight == COLOR_GREEN ? 2 : 0);
	lcd_send_data(highlight == COLOR_GREEN ? 3 : 1);
	lcd_send_text("  ");

	for (int i = 0; i < lives; ++i)
		lcd_send_data(4);
	for (int i = lives; i < 3; ++i)
		lcd_send_text(" ");

	lcd_send_text("L");
	if (current_level < 10) {
		lcd_send_data('0' + current_level);
	} else {
		lcd_send_data('0' + (current_level / 10));
		lcd_send_data('0' + (current_level % 10));
	}
	lcd_send_text("/");
	if (MAX_LEVEL_CLASSIC < 10) {
		lcd_send_data('0' + MAX_LEVEL_CLASSIC);
	} else {
		lcd_send_data('0' + (MAX_LEVEL_CLASSIC / 10));
		lcd_send_data('0' + (MAX_LEVEL_CLASSIC % 10));
	}

	// Line 2: game grid on left, score on right
	lcd_send_command(DD_RAM_ADDR2);

	lcd_send_data(highlight == COLOR_BLUE ? 2 : 0);
	lcd_send_data(highlight == COLOR_BLUE ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_data(highlight == COLOR_YELLOW ? 2 : 0);
	lcd_send_data(highlight == COLOR_YELLOW ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_text(" ");

	lcd_send_text("   ");

	lcd_send_text(" ");

	lcd_send_text("S:");
	if (score < 10) {
		lcd_send_data('0' + score);
	} else if (score < 100) {
		lcd_send_data('0' + (score / 10));
		lcd_send_data('0' + (score % 10));
	} else if (score < 1000) {
		lcd_send_data('0' + (score / 100));
		lcd_send_data('0' + ((score / 10) % 10));
		lcd_send_data('0' + (score % 10));
	}
}

static void flash_color_classic(int color, int duration) {
	int freq = 0;
	switch (color) {
		case COLOR_RED:    freq = 1500; break;
		case COLOR_GREEN:  freq = 2000; break;
		case COLOR_BLUE:   freq = 1200; break;
		case COLOR_YELLOW: freq = 1800; break;
	}

	draw_board_classic(color);
	play_note(freq, duration);

	for (volatile int p = 0; p < 8000; ++p)
		for (volatile int q = 0; q < 30; ++q);

	draw_board_classic(BUTTON_NONE);

	for (volatile int p = 0; p < 5000; ++p)
		for (volatile int q = 0; q < 30; ++q);
}

// Delay function for timing (approximately 1ms per call at 16MHz)
static void delay_ms(int ms) {
	for (volatile int i = 0; i < ms; ++i)
		for (volatile int j = 0; j < 200; ++j);
}

static int get_player_input_timed(int timeout_deciseconds) {
	int button = BUTTON_NONE;

#if ENABLE_TIMEOUT
	// Timeout enabled
	int elapsed = 0;

	while (elapsed < timeout_deciseconds && button == BUTTON_NONE) {
		button = button_pressed();
		button_unlock();

		// Calculate remaining time in pixels (0-15 for 3 chars × 5 pixels)
		int remaining = timeout_deciseconds - elapsed;
		int total_pixels = (remaining * 15) / timeout_deciseconds;
		if (total_pixels > 15) total_pixels = 15;

		// Calculate individual frames for each character
		int frame_left = (total_pixels >= 5) ? 5 : total_pixels;
		int frame_middle = (total_pixels > 5) ? ((total_pixels >= 10) ? 5 : (total_pixels - 5)) : 0;
		int frame_right = (total_pixels > 10) ? (total_pixels - 10) : 0;

		lcd_send_command(CG_RAM_ADDR + 40);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_left][i]);

		lcd_send_command(CG_RAM_ADDR + 48);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_middle][i]);

		lcd_send_command(CG_RAM_ADDR + 56);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_right][i]);

		lcd_send_command(DD_RAM_ADDR2 + 7);
		lcd_send_data(5);
		lcd_send_data(6);
		lcd_send_data(7);

		lcd_send_command(DD_RAM_ADDR2 + 11);
		lcd_send_text("S:");
		if (score < 10) {
			lcd_send_data('0' + score);
		} else if (score < 100) {
			lcd_send_data('0' + (score / 10));
			lcd_send_data('0' + (score % 10));
		} else if (score < 1000) {
			lcd_send_data('0' + (score / 100));
			lcd_send_data('0' + ((score / 10) % 10));
			lcd_send_data('0' + (score % 10));
		}

		// Update countdown every decisecond (~100ms)
		delay_ms(100);
		elapsed++;
	}

	if (button == BUTTON_NONE) {
		lcd_send_command(DD_RAM_ADDR2 + 7);
		lcd_send_text("   ");

		lcd_send_command(CG_RAM_ADDR + 40);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(SKULL_CHAR[i]);
	}
#else
	// Timeout disabled - wait indefinitely for button press
	while (button == BUTTON_NONE) {
		button = button_pressed();
		button_unlock();
	}
#endif

	if (button != BUTTON_NONE) {
		flash_color_classic(button, 60);

		while (button_pressed() != BUTTON_NONE) {
			button_unlock();
		}
	}

	return button;
}

static void play_sequence_classic() {
	// Button display time: starts at 600ms, decreases by 20ms per level, min 200ms
	int display_time = 600 - (current_level - 1) * 20;
	if (display_time < 200) display_time = 200;

	for (int i = 0; i < sequence_length; ++i) {
		flash_color_classic(sequence[i], display_time / 10);
	}
}

static void game_classic() {
	init_game_chars();
	sequence_length = 0;
	current_level = 1;
	lives = 3;
	score = 0;

	lcd_send_command(CLR_DISP);
	lcd_send_line1(" TL(E)  TR(I)");
	lcd_send_line2(" BL(F)  BR(J)");

	for (volatile int p = 0; p < 25000; ++p) {
		int button = button_pressed();
		if (button == BUTTON_DOWN) {
			while (button_pressed() != BUTTON_NONE) {
				button_unlock();
			}
			break;
		}
		button_unlock();
		for (volatile int q = 0; q < 60; ++q);
	}

	lcd_send_command(CLR_DISP);
	lcd_send_line1(" CLASSIC MODE");
	lcd_send_line2("  Get Ready...");
	play_tune(TUNE_START);

	for (volatile int p = 0; p < 25000; ++p)
		for (volatile int q = 0; q < 60; ++q);

	while (current_level <= MAX_LEVEL_CLASSIC && lives > 0) {
		int colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
		sequence[sequence_length] = colors[rnd_gen(4)];
		sequence_length++;

		lcd_send_command(CLR_DISP);

		for (volatile int p = 0; p < 1000; ++p);

		draw_board_classic(BUTTON_NONE);

		for (volatile int i = 0; i < 20000; ++i);

		play_sequence_classic();

		// Calculate input timeout: starts at 15s, decreases by 0.5s per level, min 6s
		int timeout_seconds = 150 - (current_level - 1) * 5;
		if (timeout_seconds < 60) timeout_seconds = 60;

		int mistake = 0;
		for (int i = 0; i < sequence_length; ++i) {
			int player_choice = get_player_input_timed(timeout_seconds);

			if (player_choice != sequence[i]) {
				mistake = 1;
				int old_lives = lives;
				lives--;

				lcd_send_command(CLR_DISP);
				if (player_choice == BUTTON_NONE) {
					lcd_send_line1("  TIME OUT!");
				} else {
					lcd_send_line1("    WRONG!");
				}
				lcd_send_line2("  Lives: ");
				lcd_send_command(DD_RAM_ADDR2 + 9);
				for (int j = 0; j < old_lives; ++j)
					lcd_send_data(4);

				play_note(500, 300);

				for (volatile int p = 0; p < 8000; ++p)
					for (volatile int q = 0; q < 60; ++q);

				animate_heart_break(old_lives, lives);

				for (volatile int p = 0; p < 15000; ++p)
					for (volatile int q = 0; q < 60; ++q);

				break;
			}
		}

		if (lives == 0) {
			if (current_level - 1 > best_classic_level ||
			    (current_level - 1 == best_classic_level && score > best_classic_score)) {
				best_classic_level = current_level - 1;
				best_classic_score = score;
			}

			lcd_send_command(CLR_DISP);
			lcd_send_line1("  GAME OVER!");
			lcd_send_line2("  Score: ");
			lcd_send_command(DD_RAM_ADDR2 + 9);
			if (score < 100) {
				lcd_send_data('0' + (score / 10));
				lcd_send_data('0' + (score % 10));
			} else {
				lcd_send_data('0' + (score / 100));
				lcd_send_data('0' + ((score / 10) % 10));
				lcd_send_data('0' + (score % 10));
			}

			play_tune(TUNE_GAMEOVER);

			for (volatile int p = 0; p < 30000; ++p)
				for (volatile int q = 0; q < 100; ++q);

			while (button_pressed() == BUTTON_NONE) {
				button_unlock();
			}
			while (button_pressed() != BUTTON_NONE) {
				button_unlock();
			}
			return;
		}

		if (!mistake) {
			score += current_level * 10;
			current_level++;

			if (current_level > MAX_LEVEL_CLASSIC) {
				if (MAX_LEVEL_CLASSIC > best_classic_level ||
				    (MAX_LEVEL_CLASSIC == best_classic_level && score > best_classic_score)) {
					best_classic_level = MAX_LEVEL_CLASSIC;
					best_classic_score = score;
				}

				lcd_send_command(CLR_DISP);
				lcd_send_line1("   YOU WIN!");
				lcd_send_line2("  Score: ");
				lcd_send_command(DD_RAM_ADDR2 + 9);
				if (score < 100) {
					lcd_send_data('0' + (score / 10));
					lcd_send_data('0' + (score % 10));
				} else {
					lcd_send_data('0' + (score / 100));
					lcd_send_data('0' + ((score / 10) % 10));
					lcd_send_data('0' + (score % 10));
				}

				play_tune(TUNE_LEVELUP);

				for (volatile int p = 0; p < 50000; ++p) {
					int button = button_pressed();
					if (button != BUTTON_NONE) {
						while (button_pressed() != BUTTON_NONE) {
							button_unlock();
						}
						return;
					}
					button_unlock();
					for (volatile int q = 0; q < 100; ++q);
				}

				while (button_pressed() == BUTTON_NONE) {
					button_unlock();
				}
				while (button_pressed() != BUTTON_NONE) {
					button_unlock();
				}
				return;
			}

			play_tune(TUNE_LEVELUP);

			for (volatile int p = 0; p < 10000; ++p)
				for (volatile int q = 0; q < 60; ++q);
		}
	}
}

static void draw_board_hardcore(int highlight) {
	lcd_send_command(DD_RAM_ADDR);

	// Line 1: game grid on left, best + level on right
	lcd_send_data(highlight == COLOR_RED ? 2 : 0);
	lcd_send_data(highlight == COLOR_RED ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_data(highlight == COLOR_GREEN ? 2 : 0);
	lcd_send_data(highlight == COLOR_GREEN ? 3 : 1);
	lcd_send_text("  ");

	// Personal best score for hardcore
	lcd_send_text("B:");
	if (best_hardcore < 10) {
		lcd_send_data('0' + best_hardcore);
	} else if (best_hardcore < 100) {
		lcd_send_data('0' + (best_hardcore / 10));
		lcd_send_data('0' + (best_hardcore % 10));
	} else {
		lcd_send_text("99");
	}

	lcd_send_text(" L");
	if (current_level < 10) {
		lcd_send_data('0' + current_level);
	} else if (current_level < 100) {
		lcd_send_data('0' + (current_level / 10));
		lcd_send_data('0' + (current_level % 10));
	} else {
		lcd_send_text("99");
	}

	// Line 2: game grid on left, timer + score on right
	lcd_send_command(DD_RAM_ADDR2);

	lcd_send_data(highlight == COLOR_BLUE ? 2 : 0);
	lcd_send_data(highlight == COLOR_BLUE ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_data(highlight == COLOR_YELLOW ? 2 : 0);
	lcd_send_data(highlight == COLOR_YELLOW ? 3 : 1);
	lcd_send_text(" ");
	lcd_send_text(" ");

	lcd_send_text("   ");

	lcd_send_text(" ");

	lcd_send_text("S:");
	if (score < 10) {
		lcd_send_data('0' + score);
	} else if (score < 100) {
		lcd_send_data('0' + (score / 10));
		lcd_send_data('0' + (score % 10));
	} else if (score < 1000) {
		lcd_send_data('0' + (score / 100));
		lcd_send_data('0' + ((score / 10) % 10));
		lcd_send_data('0' + (score % 10));
	} else {
		lcd_send_text("999");
	}
}

static void flash_color_hardcore(int color, int duration) {
	int freq = 0;
	switch (color) {
		case COLOR_RED:    freq = 1500; break;
		case COLOR_GREEN:  freq = 2000; break;
		case COLOR_BLUE:   freq = 1200; break;
		case COLOR_YELLOW: freq = 1800; break;
	}

	draw_board_hardcore(color);
	play_note(freq, duration);

	for (volatile int p = 0; p < 8000; ++p)
		for (volatile int q = 0; q < 30; ++q);

	draw_board_hardcore(BUTTON_NONE);

	for (volatile int p = 0; p < 5000; ++p)
		for (volatile int q = 0; q < 30; ++q);
}

static void play_sequence_hardcore() {
	// Button display time: starts at 600ms, decreases by 20ms per level, min 200ms
	int display_time = 600 - (current_level - 1) * 20;
	if (display_time < 200) display_time = 200;

	for (int i = 0; i < sequence_length; ++i) {
		flash_color_hardcore(sequence[i], display_time / 10);
	}
}

static int get_player_input_timed_hardcore(int timeout_deciseconds) {
	int button = BUTTON_NONE;

#if ENABLE_TIMEOUT
	// Timeout enabled
	int elapsed = 0;

	while (elapsed < timeout_deciseconds && button == BUTTON_NONE) {
		button = button_pressed();
		button_unlock();

		// Calculate remaining time in pixels (0-15 for 3 chars × 5 pixels)
		int remaining = timeout_deciseconds - elapsed;
		int total_pixels = (remaining * 15) / timeout_deciseconds;
		if (total_pixels > 15) total_pixels = 15;

		// Calculate individual frames for each character
		int frame_left = (total_pixels >= 5) ? 5 : total_pixels;
		int frame_middle = (total_pixels > 5) ? ((total_pixels >= 10) ? 5 : (total_pixels - 5)) : 0;
		int frame_right = (total_pixels > 10) ? (total_pixels - 10) : 0;

		lcd_send_command(CG_RAM_ADDR + 40);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_left][i]);

		lcd_send_command(CG_RAM_ADDR + 48);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_middle][i]);

		lcd_send_command(CG_RAM_ADDR + 56);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(TIMER_HORIZONTAL[frame_right][i]);

		lcd_send_command(DD_RAM_ADDR2 + 7);
		lcd_send_data(5);
		lcd_send_data(6);
		lcd_send_data(7);

		lcd_send_command(DD_RAM_ADDR2 + 11);
		lcd_send_text("S:");
		if (score < 10) {
			lcd_send_data('0' + score);
		} else if (score < 100) {
			lcd_send_data('0' + (score / 10));
			lcd_send_data('0' + (score % 10));
		} else if (score < 1000) {
			lcd_send_data('0' + (score / 100));
			lcd_send_data('0' + ((score / 10) % 10));
			lcd_send_data('0' + (score % 10));
		}

		// Update countdown every decisecond (~100ms)
		delay_ms(100);
		elapsed++;
	}

	// Only clear timer if timeout occurred
	if (button == BUTTON_NONE) {
		lcd_send_command(DD_RAM_ADDR2 + 7);
		lcd_send_text("   ");

		lcd_send_command(CG_RAM_ADDR + 40);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(SKULL_CHAR[i]);
	}
#else
	// Timeout disabled - wait indefinitely for button press
	while (button == BUTTON_NONE) {
		button = button_pressed();
		button_unlock();
	}
#endif

	if (button != BUTTON_NONE) {
		flash_color_hardcore(button, 60);

		while (button_pressed() != BUTTON_NONE) {
			button_unlock();
		}
	}

	return button;
}

static void game_hardcore() {
	init_game_chars();
	sequence_length = 0;
	current_level = 1;
	score = 0;

	lcd_send_command(CLR_DISP);
	lcd_send_line1(" TL(E)  TR(I)");
	lcd_send_line2(" BL(F)  BR(J)");

	for (volatile int p = 0; p < 25000; ++p) {
		int button = button_pressed();
		if (button == BUTTON_DOWN) {
			while (button_pressed() != BUTTON_NONE) {
				button_unlock();
			}
			break;
		}
		button_unlock();
		for (volatile int q = 0; q < 60; ++q);
	}

	lcd_send_command(CLR_DISP);
	lcd_send_data(5);
	lcd_send_text(" HARDCORE MODE ");
	lcd_send_data(5);
	lcd_send_line2("  Get Ready...");
	play_tune(TUNE_START);

	for (volatile int p = 0; p < 25000; ++p)
		for (volatile int q = 0; q < 60; ++q);

	while (1) {
		int colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
		sequence[sequence_length] = colors[rnd_gen(4)];
		sequence_length++;

		lcd_send_command(CLR_DISP);

		for (volatile int p = 0; p < 1000; ++p);

		draw_board_hardcore(BUTTON_NONE);

		for (volatile int i = 0; i < 20000; ++i);

		play_sequence_hardcore();

		// Calculate input timeout: starts at 15s, decreases by 0.5s per level, min 6s
		int timeout_seconds = 150 - (current_level - 1) * 5;
		if (timeout_seconds < 60) timeout_seconds = 60;

		for (int i = 0; i < sequence_length; ++i) {
			int player_choice = get_player_input_timed_hardcore(timeout_seconds);

			if (player_choice != sequence[i]) {
				int completed_level = current_level - 1;

				if (completed_level > best_hardcore) {
					best_hardcore = completed_level;
				}

				lcd_send_command(CLR_DISP);
				if (player_choice == BUTTON_NONE) {
					lcd_send_line1("  TIME OUT!");
				} else {
					lcd_send_line1("  GAME OVER!");
				}

				lcd_send_line2(" LVL:");
				lcd_send_command(DD_RAM_ADDR2 + 5);
				if (completed_level < 10) {
					lcd_send_data('0' + completed_level);
				} else {
					lcd_send_data('0' + (completed_level / 10));
					lcd_send_data('0' + (completed_level % 10));
				}
				lcd_send_text(" SCR:");
				if (score < 100) {
					lcd_send_data('0' + (score / 10));
					lcd_send_data('0' + (score % 10));
				} else if (score < 1000) {
					lcd_send_data('0' + (score / 100));
					lcd_send_data('0' + ((score / 10) % 10));
					lcd_send_data('0' + (score % 10));
				}

				play_tune(TUNE_GAMEOVER);

				for (volatile int p = 0; p < 30000; ++p)
					for (volatile int q = 0; q < 100; ++q);

				while (button_pressed() == BUTTON_NONE) {
					button_unlock();
				}
				while (button_pressed() != BUTTON_NONE) {
					button_unlock();
				}
				return;
			}
		}

		score += current_level * 20;
		current_level++;

		play_tune(TUNE_LEVELUP);

		for (volatile int p = 0; p < 10000; ++p)
			for (volatile int q = 0; q < 60; ++q);
	}
}

static void show_highscores() {
	init_game_chars();

	lcd_send_command(CLR_DISP);

	// Line 1: Classic best level and score
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_data(4);
	lcd_send_text("L:");
	if (best_classic_level == 0) {
		lcd_send_text("--");
	} else if (best_classic_level < 10) {
		lcd_send_data('0' + best_classic_level);
	} else {
		lcd_send_data('0' + (best_classic_level / 10));
		lcd_send_data('0' + (best_classic_level % 10));
	}
	lcd_send_text(" S:");
	if (best_classic_score == 0) {
		lcd_send_text("---");
	} else if (best_classic_score < 100) {
		lcd_send_data('0' + (best_classic_score / 10));
		lcd_send_data('0' + (best_classic_score % 10));
	} else {
		lcd_send_data('0' + (best_classic_score / 100));
		lcd_send_data('0' + ((best_classic_score / 10) % 10));
		lcd_send_data('0' + (best_classic_score % 10));
	}

	// Line 2: Hardcore best level and score
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_data(5);
	lcd_send_text("L:");
	if (best_hardcore == 0) {
		lcd_send_text("--");
	} else if (best_hardcore < 10) {
		lcd_send_data('0' + best_hardcore);
	} else {
		lcd_send_data('0' + (best_hardcore / 10));
		lcd_send_data('0' + (best_hardcore % 10));
	}
	lcd_send_text(" S:");

	// Calculate hardcore best score (sum of levels 1 to best_hardcore * 20)
	int hardcore_score = 0;
	for (int i = 1; i <= best_hardcore; ++i) {
		hardcore_score += i * 20;
	}

	if (hardcore_score == 0) {
		lcd_send_text("---");
	} else if (hardcore_score < 100) {
		lcd_send_data('0' + (hardcore_score / 10));
		lcd_send_data('0' + (hardcore_score % 10));
	} else {
		lcd_send_data('0' + (hardcore_score / 100));
		lcd_send_data('0' + ((hardcore_score / 10) % 10));
		lcd_send_data('0' + (hardcore_score % 10));
	}

	while (button_pressed() == BUTTON_NONE) {
		button_unlock();
	}
	while (button_pressed() != BUTTON_NONE) {
		button_unlock();
	}
}

// SPLASH SCREEN ------------------------------------------------------------

static void show_splash() {
	// Create animated border characters
	unsigned char anim_chars[3][8] = {
		{0b11111, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111},
		{0b00000, 0b01110, 0b01010, 0b01010, 0b01010, 0b01010, 0b01110, 0b00000},
		{0b00000, 0b00000, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00000}
	};

	for (int frame = 0; frame < 3; ++frame) {
		lcd_send_command(CG_RAM_ADDR);
		for (int i = 0; i < 8; ++i)
			lcd_send_data(anim_chars[frame][i]);

		lcd_send_command(CLR_DISP);
		lcd_send_command(DD_RAM_ADDR);

		lcd_send_data(0);
		for (int i = 0; i < 14; ++i)
			lcd_send_data(0);
		lcd_send_data(0);

		lcd_send_command(DD_RAM_ADDR + 2);
		lcd_send_text(" SIMON SAYS ");

		lcd_send_command(DD_RAM_ADDR2);
		lcd_send_data(0);
		for (int i = 0; i < 14; ++i)
			lcd_send_data(0);
		lcd_send_data(0);

		lcd_send_command(DD_RAM_ADDR2 + 2);
		lcd_send_text("by L. Bajnai ");

		for (volatile int p = 0; p < 10000; ++p)
			for (volatile int q = 0; q < 50; ++q);
	}

	play_tune(TUNE_START);

	for (volatile int p = 0; p < 15000; ++p)
		for (volatile int q = 0; q < 100; ++q);
}

// MAIN PROGRAM --------------------------------------------------------------

int main() {
	port_init();
	lcd_init();
	rnd_init();

	show_splash();

	while (1) {
		int menu_choice = show_menu();

		switch (menu_choice) {
			case MENU_CLASSIC:
				game_classic();
				break;
			case MENU_HARDCORE:
				game_hardcore();
				break;
			case MENU_HIGHSCORES:
				show_highscores();
				break;
		}
	}
}
