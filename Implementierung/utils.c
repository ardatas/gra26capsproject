#include "utils.h"
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

void print_usage(FILE *stream) {
    fprintf(stream, "Usage: ./project [options]\n");
    fprintf(stream, "Try './project --help' for a description of all options.\n");
}

int input_error(const char *message) {
    fprintf(stderr, "%s\n", message);
    print_usage(stderr);
    return EXIT_FAILURE;
}

static void print_help(void) {
    printf("Help Message\n");
    printf("  -V <0..4>                   implementation version (default is 0)\n");
    printf("      0                       SIMD with count-subtraction optimization + SIMD pixel writing\n");
    printf("      1                       scalar reference\n");
    printf("      2                       SIMD with count-subtraction optimization\n"); // TODO remove 2 through 4
    printf(
        "      3                       SIMD with count-subtraction optimization + mm_movemask only on every kth-iteration\n");
    printf("      4                       baseline SIMD\n");
    printf("  -B <number>                 runtime measurement iterations (default is 0)\n");
    printf("  -s <real>,<imag>            start point\n");
    printf("  -d <width>,<height>         image dimensions in pixels\n");
    printf("  -n <number>                 maximum iterations per pixel (0 through INT_MAX)\n");
    printf("  -i, --check-interval <K>   check fully escaped SIMD blocks every K iterations\n");
    printf("                              default is 1; supported by V1 only\n");
    printf("  -r <number>                 step per pixel\n");
    printf("  -c <real>,<imag>            julia constant c\n");
    printf("  -o <filename>               output BMP filename\n");
    printf("  -C, --color                 enable color output (default is grayscale)\n");
    printf("  -t, --test                  run all predefined benchmarking scenarios and exit\n");
    printf("  -h, --help                  show this help message\n");
    printf("  Example: ./project -V 1 -i 8 -B 100 -o output.bmp\n");
}

static bool is_allowed_output_filename(const char *filename) {
    return filename != NULL && filename[0] != '\0';
}

static int argument_error(const char *message) {
    fprintf(stderr, "%s\n", message);
    print_usage(stderr);
    return EXIT_FAILURE;
}

static bool parse_long_until(const char *text, char **end, long *value) {
    errno = 0;
    const long parsed = strtol(text, end, 10);

    if (errno != 0 || *end == text) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_float_until(const char *text, char **end, float *value) {
    errno = 0;
    const float parsed = strtof(text, end);

    if (errno != 0 || *end == text) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_int_arg(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    if (!parse_long_until(text, &end, &parsed) || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *value = (int) parsed;
    return true;
}

static bool parse_unsigned_arg(const char *text, unsigned *value) {
    char *end = NULL;
    long parsed;

    if (!parse_long_until(text, &end, &parsed) || *end != '\0' ||
        parsed < 0 || parsed > UINT_MAX) {
        return false;
    }

    *value = (unsigned) parsed;
    return true;
}

static bool parse_float_arg(const char *text, float *value) {
    char *end = NULL;

    if (!parse_float_until(text, &end, value) || *end != '\0') {
        return false;
    }

    return true;
}

static bool parse_finite_float_pair_arg(const char *text, float *first, float *second) {
    char *end = NULL;
    float parsed_first;
    float parsed_second;

    if (!parse_float_until(text, &end, &parsed_first) || *end != ',') {
        return false;
    }

    const char *second_text = end + 1;
    if (!parse_float_until(second_text, &end, &parsed_second) || *end != '\0' ||
        !isfinite(parsed_first) || !isfinite(parsed_second)) {
        return false;
    }

    *first = parsed_first;
    *second = parsed_second;
    return true;
}

static bool parse_ssize_pair_arg(const char *text, ssize_t *first, ssize_t *second) {
    char *end = NULL;
    long parsed_first;
    long parsed_second;

    if (!parse_long_until(text, &end, &parsed_first) || *end != ',') {
        return false;
    }

    const char *second_text = end + 1;
    if (!parse_long_until(second_text, &end, &parsed_second) || *end != '\0') {
        return false;
    }

    *first = (ssize_t) parsed_first;
    *second = (ssize_t) parsed_second;
    return true;
}

int parse_args(int argc, char *argv[], int *version, int *benchmark_runs, float complex *c,
               float complex *start, ssize_t *width, ssize_t *height, float *res,
               unsigned *n, unsigned *check_interval, bool *check_interval_given,
               bool *color, const char **output_filename, bool *run_test, bool *should_exit) {
    static struct option long_options[] = {
        {"color", no_argument, NULL, 'C'},
        {"check-interval", required_argument, NULL, 'i'},
        {"help", no_argument, NULL, 'h'},
        {"test", no_argument, NULL, 't'},
        {0, 0, 0, 0},
    };

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "V:B:s:d:n:i:r:c:o:Cht", long_options, NULL)) != -1) {
        switch (opt) {
            case 'V':
                if (!parse_int_arg(optarg, version)) {
                    return argument_error("Invalid -V value. Expected integer");
                }
                break;
            case 'B':
                if (!parse_int_arg(optarg, benchmark_runs)) {
                    return argument_error("Invalid -B value. Expected integer");
                }
                break;
            case 's': {
                float real;
                float imag;
                if (!parse_finite_float_pair_arg(optarg, &real, &imag)) {
                    return argument_error("Invalid -s value. Expected finite real,imag");
                }
                *start = real + imag * I;
                break;
            }
            case 'd': {
                ssize_t parsed_width;
                ssize_t parsed_height;
                if (!parse_ssize_pair_arg(optarg, &parsed_width, &parsed_height)) {
                    return argument_error("Invalid -d format. Expected width,height");
                }
                *width = parsed_width;
                *height = parsed_height;
                break;
            }
            case 'n':
                if (!parse_unsigned_arg(optarg, n) || *n > (unsigned) INT_MAX) {
                    return argument_error("Invalid -n value. Expected integer from 0 through INT_MAX");
                }
                break;
            case 'i':
                if (!parse_unsigned_arg(optarg, check_interval) || *check_interval == 0) {
                    return argument_error("Invalid -i value. Expected positive integer");
                }
                *check_interval_given = true;
                break;
            case 'r':
                if (!parse_float_arg(optarg, res)) {
                    return argument_error("Invalid -r value. Expected floating-point number");
                }
                break;
            case 'c': {
                float real;
                float imag;
                if (!parse_finite_float_pair_arg(optarg, &real, &imag)) {
                    return argument_error("Invalid -c value. Expected finite real,imag");
                }
                *c = real + imag * I;
                break;
            }
            case 'o':
                if (!is_allowed_output_filename(optarg)) {
                    return argument_error("Invalid output filename. The filename must not be empty");
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
                if (optind > 0 && optind <= argc) {
                    fprintf(stderr, "Invalid or incomplete option: %s\n", argv[optind - 1]);
                } else {
                    fprintf(stderr, "Invalid or incomplete command-line option\n");
                }
                print_usage(stderr);
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected positional argument: %s\n", argv[optind]);
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int write_bmp(const char *filename, ssize_t width, ssize_t height, bool color, const unsigned char *img) {
    const size_t image_width = (size_t) width;
    const size_t image_height = abs_height(height);
    const uint16_t bits_per_pixel = color ? 32 : 8;
    const size_t bytes_per_pixel = color ? 4 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_length = raw_row_length + padding;
    const size_t palette_size = color ? 0 : 256 * 4;
    const size_t pixel_data_size = row_length * image_height;
    const size_t pixel_offset = 14 + 40 + palette_size;
    const size_t file_size = pixel_offset + pixel_data_size;

    if (width > INT32_MAX || height < -(ssize_t) INT32_MAX || height > INT32_MAX ||
        file_size > UINT32_MAX || pixel_data_size > UINT32_MAX) {
        fprintf(stderr, "Image dimensions are too large for BMP output\n");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file for writing: %s\n", filename);
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
        fprintf(stderr, "Could not write to file: %s\n", filename);
        fclose(file);
        return EXIT_FAILURE;
    }

    // TODO note in the projektbericht the difference between defining a color palette and just using 32 bit for non color
    if (!color) {
        // color palette with grayscale (0x000000 to 0xFFFFFF with equal r g and b values)
        for (uint32_t i = 0; i < 256; ++i) {
            if (fputc((int) i, file) == EOF ||
                fputc((int) i, file) == EOF ||
                fputc((int) i, file) == EOF ||
                fputc(0, file) == EOF) {
                fprintf(stderr, "Could not write to file: %s\n", filename);
                fclose(file);
                return EXIT_FAILURE;
            }
        }
    }

    if (fwrite(img, 1, pixel_data_size, file) != pixel_data_size) {
        fprintf(stderr, "Could not write to file: %s\n", filename);
        fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "Could not close file: %s\n", filename);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
