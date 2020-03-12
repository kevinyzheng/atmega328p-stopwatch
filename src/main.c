/* Kevin Zheng and William Chi
 * ECE 231: Introduction to Embedded Systems
 * Baird Soules
 * 5 March 2020
 * Lab 2: Stopwatch
 * Program 8: Stopwatch (with start/stop and reset button)
 */

#define OCR_4MS 0x03E8		// value to store in OCR1A for 100ms

#define STARTSTOP 0			// pin 2/PD0
#define RESET 3				// pin 5/PD3

#define DIGIT1 4			// PC4: rightmost, least significant digit
#define DIGIT2 3			// PC3: second least significant digit
#define DIGIT3 2			// PC2: second most significant digit
#define DIGIT4 1			// PC1: leftmost, most significant digit

#define SS 2				// PB2: secondary select
#define MOSI 3				// PB3: main out secondary in
#define MISO 4				// PB4: main in secondary out
#define SCK 5				// PB5: SPI clock

#include <avr/io.h>
#include <asf.h>

uint8_t minutes;
uint8_t ten_seconds;
uint8_t seconds;
uint8_t tenth_seconds;
uint8_t twenty_milliseconds;

void timercounter1_init(void);
void timercounter1_start_timer(void);
void timercounter1_reset_timer(void);

void time_values_init(void);
void display_time_values(void);

void io_init(void);

unsigned char check_startstop_state(void);
void wait_startstop_state(void);
unsigned char check_reset_state(void);

void distribute_tenth_seconds(void);
uint8_t digit_lookup(uint8_t number);

void spi_main_init(void);
void spi_transmit(uint8_t data);

int main(void)
{
	timercounter1_init();
	io_init();
	spi_main_init();
	time_values_init();
	
	while(1)
	{			
		if(check_startstop_state())			// start/continue timer
		{
			wait_startstop_state();			// wait until button is released
			while(!check_startstop_state())
			{
				if(check_startstop_state()){			// check if button is pressed
					wait_startstop_state(); break; }	// wait until button is released and break
				
				display_time_values();			// 16ms
				timercounter1_start_timer();	// 4ms
				timercounter1_reset_timer();
				
				if(check_startstop_state()){
					wait_startstop_state(); break; }
						
				twenty_milliseconds++;
				
				if(check_startstop_state()){
					wait_startstop_state(); break; }
								
				if (twenty_milliseconds>=5)		// true if time elapsed is 100ms
				{
					twenty_milliseconds = 0;
					tenth_seconds++;			// increment tenth seconds (100ms)
					distribute_tenth_seconds();
					
					if(check_startstop_state()){
						wait_startstop_state(); break; }
				}
			if(check_startstop_state()){
				wait_startstop_state(); break; }	
			}
		}
		display_time_values();
		if(check_reset_state())			// reset timer
		{
			time_values_init();
			display_time_values();
			continue;
		}
	}
}

void timercounter1_init(void) 
{
	// Timer/Counter1 Control Register
	TCCR1B |= (1<<CS11) ;		// Set prescaler to 8 using Clock Select bits
	
	// Output Compare Register - 
	/* Timer Frequency = 16 MHz/8/8 = 250000 Hz
	 * Timer Interval = 1/250000 = 0.004 ms
	 * 4ms/0.004ms = 1000 = (hex) 03E8
	 */
	OCR1A = OCR_4MS;		// TCNT1 will be compared to this value
	
	// Timer/Counter Register - counts timer "ticks"
	TCNT1 = 0;
}

void timercounter1_start_timer(void)
{
	// Wait until Timer Interrupt Flag Register (overflow) and Output Compare Interrupt Flag (ticks=OCR) are set
	while((TIFR1 & (1<<OCF1A)) == 0);
}

void timercounter1_reset_timer(void)
{
	// Reset Timer/Counter Register
	TCNT1 = 0;
	// Reset Timer Interrupt Flag Register
	TIFR1 |= (1<<OCF1A);
}

void time_values_init(void)
{
	minutes = 0;
	ten_seconds = 0;
	seconds = 0;
	tenth_seconds = 0;
	twenty_milliseconds = 0;
}

void display_time_values(void){
	// tenth seconds
	PORTC |= (1 << DIGIT1);											// force digit 1 high (to be used)
	PORTC &= ~(1 << DIGIT2) & ~(1 << DIGIT3) & ~(1 << DIGIT4);		// force digits 2,3,4 low (not used)
	spi_transmit(digit_lookup(tenth_seconds));						// determine segments to light up, transmit over SPI
	timercounter1_start_timer();									// 4ms delay
	timercounter1_reset_timer();
	
	
	// one seconds
	spi_transmit(digit_lookup(seconds)+0b10000000);					// determine segments to light up, transmit over SPI
	PORTC |= (1 << DIGIT2);											// force digit 2 high (to be used)
	PORTC &= ~(1 << DIGIT1) & ~(1 << DIGIT3) & ~(1 << DIGIT4);		// force digits 1,3,4 low (not used)
	timercounter1_start_timer();									// 4ms delay
	timercounter1_reset_timer();
	
	
	// ten seconds
	spi_transmit(digit_lookup(ten_seconds));						// determine segments to light up, transmit over SPI
	PORTC |= (1 << DIGIT3);											// force digit 3 high (to be used)
	PORTC &= ~(1 << DIGIT2) & ~(1 << DIGIT1) & ~(1 << DIGIT4);		// force digits 2,1,4 low (not used)
	timercounter1_start_timer();									// 4ms delay
	timercounter1_reset_timer();
	
	
	// minutes
	spi_transmit(digit_lookup(minutes));							// determine segments to light up, transmit over SPI
	PORTC |= (1 << DIGIT4);											// force digit 4 high (to be used)
	PORTC &= ~(1 << DIGIT2) & ~(1 << DIGIT3) & ~(1 << DIGIT1);		// force digits 2,3,1 low (not used)
	timercounter1_start_timer();									// 4ms delay
	timercounter1_reset_timer();
	
}

void io_init(void)
{
	// Inputs
	DDRD &= ~(1 << STARTSTOP);					// Set as input
	DDRD &= ~(1 << RESET);						// Set as input
	PORTD |= (1 << STARTSTOP)|(1 << RESET);		// Enable internal pull-up resistors
	
	// Outputs
	// Data Direction Register C
	DDRC |= (1<<DIGIT4)|(1<<DIGIT3)|(1<<DIGIT2)|(1<<DIGIT1);		// Set PC 1-4 as outputs
}

unsigned char check_startstop_state(void)
{
	if (!(PIND & (1<<STARTSTOP))) return 1;		// return 1 if PIND value is 0 or STARTSTOP is pressed
	return 0;
}

void wait_startstop_state(void)
{
	while(check_startstop_state()) display_time_values();
}

unsigned char check_reset_state(void)
{
	if (!(PIND & (1<<RESET))) return 1;	// return 1 if PIND value is 0 or RESET is pressed
	return 0;
}

uint8_t digit_lookup(uint8_t number)
{
	switch(number)
	{
		case 0:
			return 0b00111111;
			break;
		case 1:
			return 0b00000110;
			break;
		case 2:
			return 0b01011011;
			break;
		case 3:
			return 0b01001111;
			break;
		case 4:
			return 0b01100110;
			break;
		case 5:
			return 0b01101101;
			break;
		case 6:
			return 0b01111101;
			break;
		case 7:
			return 0b00000111;
			break;
		case 8:
			return 0b01111111;
			break;
		case 9:
			return 0b01100111;
			break;	
	}
	return 0;
}

void distribute_tenth_seconds(void)
{
	if (tenth_seconds >= 10)
	{
		tenth_seconds -= 10;
		seconds++;
		if (seconds >= 10) 
		{
			seconds -= 10;
			ten_seconds++;
			if (ten_seconds >= 6) 
			{
				ten_seconds -= 6;
				minutes++;
				if (minutes >= 10) minutes = 0;
			}
		}
	}
}

void spi_main_init(void)
{
	DDRB |= (1<<MOSI)|(1<<SCK)|(1<<SS);	// Set MOSI, SCK and SS as outputs
	
	// SPI Control Register
	SPCR = (1<<SPE)|(1<<MSTR);	// SPI Enable, set as main
	
	// SPI Status Register
	SPSR = (1<<SPI2X);			// Divide frequency by 2 to get 1MHz (16MHz/8/2 = 1MHz)
}

void spi_transmit (uint8_t data)
{
	SPDR = data;					// store output in SPI Data Register, where transfer takes place
	while(!(SPSR & (1<<SPIF)));     // wait for SPI Status Register and SPI Interrupt Flag to signal end of transmission
	
	// pulse SS
	PORTB |= (1 << SS);				// force SS high
	PORTB &= ~(1 << SS);			// force SS low
}