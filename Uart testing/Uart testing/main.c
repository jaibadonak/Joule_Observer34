#define F_CPU 2000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define RMSVoltage   1
#define PeakCurrent  3
#define Power        5

#define BAUD 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)

void USART_init(void)
{
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_transmit(char data)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void USART_string(const char *str)
{
	while (*str)
	{
		USART_transmit(*str++);
	}
}

void print_RMSVoltage(double voltage)
{
	uint16_t value = (uint16_t)(voltage * 10.0 + 0.5);
	USART_transmit(((value / 100) % 10) + '0');
	USART_transmit(((value / 10) % 10) + '0');
	USART_transmit('.');
	USART_transmit((value % 10) + '0');
}

void print_PeakCurrent(double current)
{
	uint16_t value = (uint16_t)(current + 0.5);
	USART_transmit(((value / 100) % 10) + '0');
	USART_transmit(((value / 10) % 10) + '0');
	USART_transmit((value % 10) + '0');
}

void print_Power(double power)
{
	uint16_t value = (uint16_t)(power * 100.0 + 0.5);
	USART_transmit(((value / 100) % 10) + '0');
	USART_transmit('.');
	USART_transmit(((value / 10) % 10) + '0');
	USART_transmit((value % 10) + '0');
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