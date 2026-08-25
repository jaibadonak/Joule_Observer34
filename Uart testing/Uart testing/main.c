#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define RMSVoltage   14.5
#define PeakCurrent  125
#define Power        1.60

#define BAUD 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)

void USART_init(void)
{
	// Set baud
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	// Enable transmit
	UCSR0B = (1 << TXEN0);

	// Asynchronous USART 8N1
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_transmit(char data)
{
	// Wait for transmit buffer empty
	while (!(UCSR0A & (1 << UDRE0)));

	// Send data
	UDR0 = data;
}

void USART_string(const char *str)
{
	while (*str)
	{
		USART_transmit(*str++);
	}
}

void USART_uint(uint16_t value)
{
	char buffer[6];
	uint8_t i = 0;

	// Special case for 0
	if (value == 0)
	{
		USART_transmit('0');
		return;
	}

	// Convert integer into digits backwards
	while (value > 0)
	{
		buffer[i++] = (value % 10) + '0';
		value /= 10;
	}

	// Send digits in correct order
	while (i > 0)
	{
		USART_transmit(buffer[--i]);
	}
}

void print_RMSVoltage(double voltage)
{
	// Convert XX.X into integer tenths
	uint16_t value = (uint16_t)(voltage * 10.0 + 0.5);

	// Fixed extraction to get XX.X format
	USART_transmit(((value / 100) % 10) + '0'); // Tens
	USART_transmit(((value / 10) % 10) + '0');  // Ones
	USART_transmit('.');
	USART_transmit((value % 10) + '0');         // Tenths
}

void print_PeakCurrent(double current)
{
	// Convert XXX into integer
	uint16_t value = (uint16_t)(current + 0.5);

	// Fixed extraction to get XXX format
	USART_transmit(((value / 100) % 10) + '0'); // Hundreds
	USART_transmit(((value / 10) % 10) + '0');  // Tens
	USART_transmit((value % 10) + '0');         // Ones
}

void print_Power(double power)
{
	// Convert X.XX into integer hundredths
	uint16_t value = (uint16_t)(power * 100.0 + 0.5);

	// Fixed extraction to get X.XX format
	USART_transmit(((value / 100) % 10) + '0'); // Ones
	USART_transmit('.');
	USART_transmit(((value / 10) % 10) + '0');  // Tenths
	USART_transmit((value % 10) + '0');         // Hundredths
}

int main(void)
{
	USART_init();

	while (1)
	{
		USART_string("RMS Voltage is: ");
		print_RMSVoltage(RMSVoltage);
		USART_string("\r\n");

		USART_string("Peak Current is: ");
		print_PeakCurrent(PeakCurrent);
		USART_string("\r\n");

		USART_string("Power is: ");
		print_Power(Power);
		USART_string("\r\n\r\n");

		_delay_ms(1000);
	}
}