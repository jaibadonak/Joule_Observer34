#ifndef JOULE_UART_PROTOCOL_H
#define JOULE_UART_PROTOCOL_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Validate every field, including optional fields beyond the output capacity.
// Return the total field count, or -1. Never write beyond out[maxCount - 1].
static inline int readNumbers(const char *text, long *out, int maxCount) {
    int count = 0;
    while (*text == ',') {
        const char *start = text + 1;
        const char *digit = (*start == '-' || *start == '+') ? start + 1 : start;
        if (*digit < '0' || *digit > '9') return -1;
        char *end;
        errno = 0;
        long value = strtol(start, &end, 10);
        if (errno == ERANGE || (*end != ',' && *end != '\0')) return -1;
        if (count < maxCount) out[count] = value;
        count++;
        text = end;
    }
    return *text == '\0' ? count : -1;
}

static inline int validMeasurements(const long *n, int count) {
    if (count<3 || count==6 || count==7 || n[0]<0 || n[1]<0 ||
        (count>=4 && n[3]<0) || (count>=5 && n[4]<0)) return 0;
    if (count>=8 && (n[5]<0 || n[6]<-1 || n[6]>1000 || n[7]<0)) return 0;
    return 1;
}

static inline int parseDisplayMode(const char *line) {
    if (strlen(line) != 4 || strncmp(line, "$D,", 3) != 0 ||
        line[3] < '0' || line[3] > '3') return -1;
    return line[3] - '0';
}

static inline int validCalibration(const long *c, int count) {
    if ((count != 11 && count != 13) || c[0] < 1000 || c[0] > 5500 || c[1] < 1 || c[1] > 60000 ||
        c[2] < 1 || c[2] > 60000) return 0;
    for (int k = 3; k <= 7; k += 4) {
        if (c[k] < 0 || c[k + 1] > 1023 || c[k] > c[k + 1] ||
            c[k + 2] < c[k] * 100 || c[k + 2] > c[k + 1] * 100 ||
            c[k + 3] < 0 || c[k + 3] > 10230) return 0;
    }
    if (count == 13 && (c[11] < 0 || c[11] > 51150 || c[12] < 0 || c[12] > 51150)) return 0;
    return 1;
}

static inline int validRawCapture(const long *n, int count, int maxPairs) {
    if (count < 9 || count > 5 + 2 * maxPairs || (count - 5) % 2 ||
        n[0] < 1 || n[0] > 1000000 || n[1] < 0 || n[1] > 102300 ||
        n[2] < 0 || n[2] > 102300 || n[3] < 1 || n[3] > 60000 ||
        n[4] < 1 || n[4] > 60000) return 0;
    for (int k = 5; k < count; k++) if (n[k] < 0 || n[k] > 1023) return 0;
    return 1;
}

// Call only after readNumbers with capacity 1 + stride * maxPairs.
static inline int validWaveform(const long *n, int count, int raw, int maxPairs) {
    const int stride = raw ? 4 : 2;
    if (count < 1 + 2 * stride || count > 1 + stride * maxPairs ||
        (count - 1) % stride != 0 || n[0] <= 0 || n[0] > 1000000L) return 0;
    if (raw) for (int k = 0; k < (count - 1) / stride; k++) {
        if (n[3 + stride * k] < 0 || n[3 + stride * k] > 1023 ||
            n[4 + stride * k] < 0 || n[4 + stride * k] > 1023) return 0;
    }
    return 1;
}

#endif
