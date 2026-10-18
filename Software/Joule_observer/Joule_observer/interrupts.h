#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_

// Set up INT0 (PD2): rising edge of the voltage zero crossing detector.
// The ISR itself is in vzc.c. Call this before sei().
void int0_init(void);

#endif /* INTERRUPTS_H_ */
