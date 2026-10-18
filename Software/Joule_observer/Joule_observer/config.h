#ifndef JOULE_CONFIG_H
#define JOULE_CONFIG_H

#ifndef F_CPU
#define F_CPU 2000000UL
#endif
#if F_CPU != 2000000UL
#error "Firmware timing is designed for a 2 MHz CPU clock; check clock/fuses."
#endif

#define TIMER1_TICKS_PER_SECOND (F_CPU / 64UL)

#endif
