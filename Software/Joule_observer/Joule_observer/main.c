/*
 * Joule_observer.c
 *
 * Created: 14/10/2026 5:04:55 pm
 * Author : Admin
 */ 

#include <avr/io.h>


int main(void)
{
    adc_init();
    /* Replace with your application code */
    while (1) 
    {

    //-------------INSTANTANOUS POWER-----------//
    if (conversion_ready) {
        //power calc 
        adc_start_capture();
    }






    }
}

