#ifndef JOULE_CONFIG_H
#define JOULE_CONFIG_H

#ifndef F_CPU
#define F_CPU 2000000UL
#endif
//N1
#if F_CPU != 2000000UL 
#error "Firmware timing is designed for a 2 MHz CPU clock; check clock/fuses."
#endif
//N2
#define TIMER1_TICKS_PER_SECOND (F_CPU / 64UL)

#endif


//------------------NOTES----------------
//
//UL: unsigned long (32-bit). 
//N1: if cpu freq is not 2Mhz, fail the compliation(only checks macro)
//N2: simply ticks per second defined by the fre of cpu / by prescaler 
