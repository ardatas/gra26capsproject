//
// Created by Noah Schneider on 29.06.26.
//

#ifndef GRA26CAPSPROJECT_T146_JULIA_H
#define GRA26CAPSPROJECT_T146_JULIA_H
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/_types/_ssize_t.h>

int julia(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);

#endif //GRA26CAPSPROJECT_T146_JULIA_H
