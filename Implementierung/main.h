#ifndef GRA26CAPSPROJECT_T146_MAIN_H
#define GRA26CAPSPROJECT_T146_MAIN_H

#include <complex.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    float complex c;
    float complex start;
    ssize_t width;
    ssize_t height;
    float res;
    unsigned n;
    bool color;
} CalculationParams;

enum {
    IMPLEMENTATION_COUNT = 4,
};

void calculate_version(int version, const CalculationParams *params, unsigned char *image);

int image_buffer_size(const CalculationParams *params, size_t *pixel_data_size);

#endif // GRA26CAPSPROJECT_T146_MAIN_H
