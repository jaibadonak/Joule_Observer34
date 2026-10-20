#ifndef JOULE_WAVEFORM_RECEIVER_H
#define JOULE_WAVEFORM_RECEIVER_H

#include "uart_protocol.h"

#ifndef WAVEFORM_MAX_SAMPLES
#define WAVEFORM_MAX_SAMPLES 200
#endif

// Only receive and validate AVR results. No electrical calculations happen here.
typedef struct {
    long values[5][WAVEFORM_MAX_SAMPLES]; // V mV, I mA, P mW, raw V, raw I
    long stats[5][4];                   // minimum, maximum, mean, RMS
    int count, expected, stats_count;
    long dt_us;
    uint32_t started_at;
    int active;
} waveform_receiver_t;

enum { WAVE_INVALID=-1, WAVE_OTHER=0, WAVE_PENDING=1, WAVE_COMPLETE=2 };

static inline int waveform_receive(waveform_receiver_t *frame, const char *line, uint32_t now) {
    if (strncmp(line,"$B,",3)==0) {
        long n[2];
        if (readNumbers(line+2,n,2)!=2 || n[0]<1 || n[0]>1000000 ||
            n[1]<2 || n[1]>WAVEFORM_MAX_SAMPLES) goto invalid;
        frame->dt_us=n[0]; frame->expected=n[1];
        frame->count=frame->stats_count=0;
        frame->started_at=now; frame->active=1;
        return WAVE_PENDING;
    }
    if (strncmp(line,"$S,",3)==0) {
        long n[6];
        if (!frame->active || readNumbers(line+2,n,6)!=6 || n[0]!=frame->count ||
            frame->count>=frame->expected || frame->stats_count ||
            n[4]<0 || n[4]>1023 || n[5]<0 || n[5]>1023) goto invalid;
        for (int channel=0;channel<5;channel++) frame->values[channel][frame->count]=n[channel+1];
        frame->count++;
        return WAVE_PENDING;
    }
    if (strncmp(line,"$G,",3)==0) {
        long n[5];
        if (!frame->active || frame->count!=frame->expected || readNumbers(line+2,n,5)!=5 ||
            n[0]!=frame->stats_count || frame->stats_count>=5 || n[1]>n[2] ||
            n[3]<n[1] || n[3]>n[2] || n[4]<0) goto invalid;
        for (int field=0;field<4;field++) frame->stats[frame->stats_count][field]=n[field+1];
        frame->stats_count++;
        return WAVE_PENDING;
    }
    if (strcmp(line,"$E")==0) {
        if (!frame->active || frame->count!=frame->expected || frame->stats_count!=5) goto invalid;
        frame->active=0;
        return WAVE_COMPLETE;
    }
    return WAVE_OTHER; // Mode, measurement and calibration lines can interleave.

invalid:
    frame->active=0;
    return WAVE_INVALID;
}

#endif
