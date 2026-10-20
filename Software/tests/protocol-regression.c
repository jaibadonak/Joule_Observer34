#include <stdint.h>
#define WAVEFORM_MAX_SAMPLES 3
#include "../../Application/src/waveform_receiver.h"
volatile uint16_t test_failures = 0;
volatile uint16_t failure_lines[16];
#define CHECK(condition) do { if (!(condition)) { if(test_failures<16) failure_lines[test_failures]=__LINE__; test_failures++; } } while(0)
void __attribute__((noinline)) test_finished(void) { __asm__ __volatile__("nop"); }
int main(void) {
    waveform_receiver_t receiver = {0};
    CHECK(waveform_receive(&receiver,"$B,104,2",123)==WAVE_PENDING);
    CHECK(waveform_receive(&receiver,"$S,0,-100,-10,1,400,300",124)==WAVE_PENDING);
    CHECK(waveform_receive(&receiver,"$D,1",125)==WAVE_OTHER);
    CHECK(waveform_receive(&receiver,"$S,1,100,10,2,444,422",126)==WAVE_PENDING);
    const char *stat_lines[5] = {"$G,0,-100,100,0,100","$G,1,-10,10,0,10",
        "$G,2,1,2,2,1","$G,3,400,444,422,422","$G,4,300,422,361,366"};
    for (uint8_t k=0;k<5;k++) CHECK(waveform_receive(&receiver,stat_lines[k],127)==WAVE_PENDING);
    CHECK(waveform_receive(&receiver,"$E",128)==WAVE_COMPLETE);
    CHECK(receiver.values[0][0]==-100 && receiver.values[2][1]==2 && receiver.started_at==123);
    CHECK(waveform_receive(&receiver,"$B,104,2",200)==WAVE_PENDING);
    CHECK(waveform_receive(&receiver,"$S,1,100,10,2,444,422",201)==WAVE_INVALID); // lost row
    CHECK(waveform_receive(&receiver,"$E",202)==WAVE_INVALID);
    CHECK(waveform_receive(&receiver,"$B,104,4",203)==WAVE_INVALID); // capacity
    CHECK(waveform_receive(&receiver,"$B,104,2",204)==WAVE_PENDING);
    CHECK(waveform_receive(&receiver,"$S,0,100,10,2,1024,422",205)==WAVE_INVALID);
    long derived_fields[8]={14000,400,2800,123,566,5600,500,500000};
    CHECK(validMeasurements(derived_fields,8));
    CHECK(!validMeasurements(derived_fields,7));
    derived_fields[6]=1001; CHECK(!validMeasurements(derived_fields,8));
    test_finished();
    while(1) {}
}
