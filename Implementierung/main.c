//
// Created by Noah Schneider on 29.06.26.
//

#include "julia.h"

int main(int argc, char * argv[]) {
    float complex c = -0.5125f + 0.5213f * I;
    float complex start = -2.0f + 1.5f * I;
    size_t width = 800;
    ssize_t height = -600;
    float res = 0.005f;
    unsigned n = 100;
    bool color = true;
    unsigned char* img = (unsigned char *)"output.bmp";

    // TODO read args

    julia(c, start, width, height, res, n, color, img);

    return 0;
}