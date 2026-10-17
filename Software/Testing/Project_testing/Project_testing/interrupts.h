/*
 * interrupts.h
 *
 * Created: 17/10/2026 2:36:58 pm
 *  Author: Admin
 *


#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_





#endif /* INTERRUPTS_H_ */

/*
 * interrupts.h
 *
 * Created: 15/10/2026 2:38:31 pm
 *  Author: Admin
 */ 


#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_

// Set up INT0 (PD2): rising edge of the voltage zero crossing detector.
// The ISR itself is in vzc.c. Call this before sei().
void int0_init(void);

#endif /* INTERRUPTS_H_ */