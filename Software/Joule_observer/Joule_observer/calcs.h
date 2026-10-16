/*
 * calcs.h
 *
 * Created: 15/10/2026 12:24:55 pm
 *  Author: Admin
 */ 


#ifndef CALCS_H_
#define CALCS_H_

volatile uint8_t voltage_RMS(uint16_t voltage[200]);
volatile uint8_t current_RMS(uint16_t current[200]);
volatile void compute_instPower(uint16_t voltage[200], uint16_t current[200]);




#endif /* CALCS_H_ */