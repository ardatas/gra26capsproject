//created by Erdeniz Taslicukur on 06.07.2026

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


void julia_Erdeniz(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n,
                   bool color, unsigned char *img) {
    unsigned char brightness;
    size_t heightPositive = (height > 0) ? (size_t) height : (size_t) -height;

    for (size_t y = 0; y < heightPositive; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t i = 0;
            float a = crealf(start) + res * x;
            float b = cimagf(start) + ((height < 0) ? -res : res) * y;

            while ((a * a + b * b) < 4.0f && i < n) {
                /* code */
                float new_a = (a * a) - (b * b) + crealf(c);
                float new_b = 2.0f * a * b + cimagf(c);

                a = new_a;
                b = new_b;
                i++;
            }


            unsigned char red, green, blue;
            if (!color) {
                if (i == n) {
                    brightness = 0;

                    /* code */
                } else {
                    brightness = (unsigned char) (255 - 4 * (i % 64));
                }
                size_t row_stride = (width + 3) & ~3;
                size_t index = y * width + x;
                img[index] = brightness; /* code */
            } else {
                if (i == n) {
                    red = 0;
                    green = 0;
                    blue = 0;
                } else {
                    float proportion = (float) i / (float) n;
                    red = 255.0f * (1.0 - proportion) * (1.0f - proportion) * (1.0f - proportion) * proportion * 9.0f;
                    green = 255.0f * (1.0f - proportion) * (1.0f - proportion) * proportion * proportion * 15.0f;
                    blue = 255.0f * (1.0f - proportion) * proportion * proportion * proportion * 8.5f;
                    /* code */
                }
                size_t row_stride = (width * 3 + 3) & ~3;
                size_t index = (y * width * 3 + (3 * x));
                img[index] = red;
                img[index + 1] = green;
                img[index + 2] = blue;
            }
        }
    }
}
