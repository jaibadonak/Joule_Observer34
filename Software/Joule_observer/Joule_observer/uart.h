/*
 * uart.h
 *
 * Created: 14/10/2026 5:05:18 pm
 *  Author: Admin
 */ 


#ifndef UART_H_     
#define UART_H_  

#include <stdint.h> 

// Initialise the UART hardware
void uart_init(void);

// Transmit one character (1 byte)
void uart_transmit(char data);

// Transmit complete string one char at a time
void uart_string(const char *str);

// Convert signed 32-bit int into text and transmit
void uart_int32(int32_t value);

// Send voltage, current and power measurements
void uart_send_measurements(int32_t vrms_mV,
                            int32_t irms_mA,
                            int32_t power_mW,
                            uint32_t energy_mWh);

// 32 contiguous samples/channel, 104 us apart, from frozen cycle-aligned blocks.
void uart_send_waveform(void);
void uart_send_calibration(void);

// Display mode: 0=voltage, 1=current, 2=power, 3=energy.
void uart_send_mode(uint8_t mode);

#endif /* UART_H_ */
