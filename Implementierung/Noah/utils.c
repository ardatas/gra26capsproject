#include "utils.h"
#include <getopt.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPFileHeader;

typedef struct __attribute__((packed)) {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;

size_t abs_height(ssize_t height) {
    return height < 0 ? (size_t) -height : (size_t) height;
}

static void print_help(void) {
    printf("Help Message\n");
    printf("  -V <number>                 implementation version (default is 0)\n");
    printf("  -B <number>                 benchmark repetitions (default is 1)\n");
    printf("  -s <real>,<imag>            start point\n");
    printf("  -d <width>,<height>         image dimensions in pixels\n");
    printf("  -n <number>                 maximum iterations per pixel\n");
    printf("  -r <number>                 resolution per pixel\n");
    printf("  -c <real>,<imag>            julia constant c\n");
    printf("  -o <filename>               output BMP file\n");
    printf("  -C, --color                 enable color output\n");
    printf("  -t, --test                  run tests\n");
    printf("  -h, --help                  show this help\n");
}

static bool is_allowed_output_filename(const char* filename) {
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }

    regex_t regex;
    if (regcomp(&regex, "^[A-Za-z0-9._/-]+$", REG_EXTENDED) != 0) {
        return false;
    }

    const bool matches = regexec(&regex, filename, 0, NULL, 0) == 0;
    regfree(&regex);

    return matches && strstr(filename, "..") == NULL;
}

static bool parse_long_until(const char* text, char** end, long* value) {
    errno = 0;
    const long parsed = strtol(text, end, 10);

    if (errno != 0 || *end == text) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_float_until(const char* text, char** end, float* value) {
    errno = 0;
    const float parsed = strtof(text, end);

    if (errno != 0 || *end == text) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_int_arg(const char* text, int* value) {
    char* end = NULL;
    long parsed;

    if (!parse_long_until(text, &end, &parsed) || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *value = (int) parsed;
    return true;
}

static bool parse_unsigned_arg(const char* text, unsigned* value) {
    char* end = NULL;
    long parsed;

    if (!parse_long_until(text, &end, &parsed) || *end != '\0' ||
        parsed < 0 || parsed > UINT_MAX) {
        return false;
    }

    *value = (unsigned) parsed;
    return true;
}

static bool parse_float_arg(const char* text, float* value) {
    char* end = NULL;

    if (!parse_float_until(text, &end, value) || *end != '\0') {
        return false;
    }

    return true;
}

static bool parse_float_pair_arg(const char* text, float* first, float* second) {
    char* end = NULL;
    float parsed_first;
    float parsed_second;

    if (!parse_float_until(text, &end, &parsed_first) || *end != ',') {
        return false;
    }

    const char* second_text = end + 1;
    if (!parse_float_until(second_text, &end, &parsed_second) || *end != '\0') {
        return false;
    }

    *first = parsed_first;
    *second = parsed_second;
    return true;
}

static bool parse_ssize_pair_arg(const char* text, ssize_t* first, ssize_t* second) {
    char* end = NULL;
    long parsed_first;
    long parsed_second;

    if (!parse_long_until(text, &end, &parsed_first) || *end != ',') {
        return false;
    }

    const char* second_text = end + 1;
    if (!parse_long_until(second_text, &end, &parsed_second) || *end != '\0') {
        return false;
    }

    *first = (ssize_t) parsed_first;
    *second = (ssize_t) parsed_second;
    return true;
}

int parse_args(int argc, char* argv[], int* version, int* benchmark_runs, float complex* c,
               float complex* start, ssize_t* width, ssize_t* height, float* res,
               unsigned* n, bool* color, const char** output_filename, bool* run_test,
               bool* should_exit) {
    static struct option long_options[] = {
        {"color", no_argument, NULL, 'C'},
        {"help", no_argument, NULL, 'h'},
        {"test", no_argument, NULL, 't'},
        {0, 0, 0, 0},
    };

    optind = 1;
    int opt;
    while ((opt = getopt_long(argc, argv, "V:B:s:d:n:r:c:o:Cht", long_options, NULL)) != -1) {
        switch (opt) {
            case 'V':
                if (!parse_int_arg(optarg, version)) {
                    fprintf(stderr, "Invalid -V value. Expected integer\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'B':
                if (!parse_int_arg(optarg, benchmark_runs)) {
                    fprintf(stderr, "Invalid -B value. Expected integer\n");
                    return EXIT_FAILURE;
                }
                break;
            case 's': {
                float real;
                float imag;
                if (!parse_float_pair_arg(optarg, &real, &imag)) {
                    fprintf(stderr, "Invalid -s format. Expected real,imag\n");
                    return EXIT_FAILURE;
                }
                *start = real + imag * I;
                break;
            }
            case 'd': {
                ssize_t parsed_width;
                ssize_t parsed_height;
                if (!parse_ssize_pair_arg(optarg, &parsed_width, &parsed_height)) {
                    fprintf(stderr, "Invalid -d format. Expected width,height\n");
                    return EXIT_FAILURE;
                }
                *width = parsed_width;
                *height = parsed_height;
                break;
            }
            case 'n':
                if (!parse_unsigned_arg(optarg, n)) {
                    fprintf(stderr, "Invalid -n value. Expected non-negative integer\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'r':
                if (!parse_float_arg(optarg, res)) {
                    fprintf(stderr, "Invalid -r value. Expected floating-point number\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'c': {
                float real;
                float imag;
                if (!parse_float_pair_arg(optarg, &real, &imag)) {
                    fprintf(stderr, "Invalid -c format. Expected real,imag\n");
                    return EXIT_FAILURE;
                }
                *c = real + imag * I;
                break;
            }
            case 'o':
                if (!is_allowed_output_filename(optarg)) {
                    fprintf(stderr, "Invalid output filename. Allowed characters: A-Z a-z 0-9 . _ - /\n");
                    return EXIT_FAILURE;
                }
                *output_filename = optarg;
                break;
            case 'C':
                *color = true;
                break;
            case 't':
                *run_test = true;
                break;
            case 'h':
                print_help();
                *should_exit = true;
                break;
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int write_bmp(const char* filename, ssize_t width, ssize_t height, bool color, const unsigned char* img) {
    const size_t image_width = (size_t) width;
    const size_t image_height = abs_height(height);
    const uint16_t bits_per_pixel = color ? 24 : 8;
    const size_t bytes_per_pixel = color ? 3 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_length = raw_row_length + padding;
    const size_t palette_size = color ? 0 : 256 * 4;
    const size_t pixel_data_size = row_length * image_height;
    const size_t pixel_offset = 14 + 40 + palette_size;
    const size_t file_size = pixel_offset + pixel_data_size;

    if (width > INT32_MAX || height < INT32_MIN || height > INT32_MAX ||
        file_size > UINT32_MAX || pixel_data_size > UINT32_MAX) {
        fprintf(stderr, "Image dimensions are too large for BMP output\n");
        return EXIT_FAILURE;
    }

    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        perror(filename);
        return EXIT_FAILURE;
    }

    BMPFileHeader file_header = {
        // "BM"
        .type = 0x4d42,
        .size = (uint32_t) file_size,
        .reserved1 = 0,
        .reserved2 = 0,
        .offset = (uint32_t) pixel_offset,
    };
    BMPInfoHeader info_header = {
        .size = sizeof(BMPInfoHeader),
        .width = (int32_t) width,
        .height = (int32_t) height,
        .planes = 1,
        .bit_count = bits_per_pixel,
        .compression = 0,
        .image_size = (uint32_t) pixel_data_size,
        .x_pixels_per_meter = 0,
        .y_pixels_per_meter = 0,
        .colors_used = 0,
        .colors_important = 0,
    };

    if (fwrite(&file_header, sizeof(file_header), 1, file) != 1 ||
        fwrite(&info_header, sizeof(info_header), 1, file) != 1) {
        perror(filename);
        fclose(file);
        return EXIT_FAILURE;
    }

    if (!color) {
        // color palette with grayscale (0x000000 to 0xFFFFFFFF with equal r g and b values)
        for (uint32_t i = 0; i < 256; ++i) {
            fputc((int) i, file);
            fputc((int) i, file);
            fputc((int) i, file);
            fputc(0, file);
        }
    }

    if (fwrite(img, 1, pixel_data_size, file) != pixel_data_size) {
        perror(filename);
        fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        perror(filename);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
