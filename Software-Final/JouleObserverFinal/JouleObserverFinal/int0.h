// Include guard: prevents these declarations being included more than once.
#ifndef INT0_H_
#define INT0_H_

// Declaration for the function defined in int0.c, so main.c can call it.
// Set up the voltage zero crossing input before enabling interrupts.
void int0_init(void);

#endif /* INT0_H_ */
