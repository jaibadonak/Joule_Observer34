#ifndef JOULE_UART_PROTOCOL_H
#define JOULE_UART_PROTOCOL_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

static inline int parseDisplayMode(const char *line) {
    if (strlen(line) != 4 || strncmp(line, "$D,", 3) != 0 ||
        line[3] < '0' || line[3] > '3') return -1;
    return line[3] - '0';
}

static inline int validCalibration(const long *c, int count) {
    if (count != 11 || c[0] < 1000 || c[0] > 5500 || c[1] < 1 || c[1] > 60000 ||
        c[2] < 1 || c[2] > 60000) return 0;
    for (int k = 3; k <= 7; k += 4) {
        if (c[k] < 0 || c[k + 1] > 1023 || c[k] > c[k + 1] ||
            c[k + 2] < c[k] * 100 || c[k + 2] > c[k + 1] * 100 ||
            c[k + 3] < 0 || c[k + 3] > 10230) return 0;
    }
    return 1;
}

#endif
