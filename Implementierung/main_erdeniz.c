#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <complex.h>
#include <getopt.h>
#include <sys/types.h>
#include "julia.h"

typedef struct{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
}
BMPFileHeader;


typedef struct{
uint32_t biSize;
int32_t biWidth;
int32_t biHeight;
uint16_t biPlanes;
uint16_t biBitCount;
uint32_t biCompression;
uint32_t biSizeImage;
int32_t biXpelsPerMeter;
int32_t biYpelsPerMeter;
uint32_t biClrUsed;
uint32_t biClrImportant;
}
BMPInfoHeader;

void print_help(){
    printf("Help Message\n");
    printf(" -V <int>                        Implementation version to use\n" );
    printf("-B <int>                         Number of benchmark repetitions\n");
    printf("-s <RealPart>, <ImaginaryPart>   starting point in complex plane\n");
    printf("-d <width>, <height>             image dimensions               \n");
    printf("-n <int>                         maximum number of iterations per pixel\n");
    printf("-r <float>                       resolution\n                      ");
    printf("c <real>, <imag>                 complex number c                  ");
    printf("o  <filename>                    output                            ");
    printf("C  --color                       enable color output\n              ");
    printf("-t --test                      execute the tests                  ");          
    printf("-h --help                     display the help message and exit\n");

}


int main(int argc, char * argv[]) {

    int version=0;
    int benchmark_number=0;
    float complex c = -0.5125f + 0.5213f * I;
    float complex start = -2.0f + 1.5f * I;
    size_t width = 600;
    ssize_t height = -400;
    float res = 0.01f;
    unsigned n = 255;
    bool color = false;
    char *output_filename = "output.bmp";
    bool run_test = false;



if (img==NULL){
    fprintf(stderr, "Allocation failed\n");
    return EXIT_FAILURE;
}
    



struct option long_options[]= {
    {"color", no_argument,  NULL, 'C'},
    {"help", no_argument,  NULL, 'h'},
    {"test", no_argument,  NULL, 't'},
    {0, 0,  0, 0},
};

 int opt;
 float r_part;
 float i_part;

    /* data */
    // TODO read args
while ((opt= getopt_long(argc,argv, "V:B:s:d:n:r:c:o:Cht", long_options, NULL))!=-1)
{
    switch (opt){
case 'V':
version= atoi(optarg);
break;

case 'B':
benchmark_number= atoi(optarg);
break;

case 's':
if (sscanf(optarg, "%f,%f", &r_part, &i_part)==2){
start= r_part+ i_part*I;
}
else
{
   fprintf(stderr, "Invalid format for -s. Expected: real, imag\n");
   exit(EXIT_FAILURE); /* code */
}
break;

case 'd': {
long long temp_width;
long long temp_height;
if (sscanf(optarg, "%lld, %lld", &temp_width, &temp_height)==2)
{
    width= (size_t) temp_width;
    height= (ssize_t) temp_height;
 /* code */
}
else
{
    fprintf(stderr, "Error: Invalid input for -d. Expected: width, height\n");
    exit(EXIT_FAILURE);
 /* code */
}
break;
}


case 'n':
n=(unsigned int) atoi(optarg);
break;


case 'r':
res=strtof(optarg, NULL);
break;


case 'c':
if (sscanf(optarg, "%f,%f", &r_part, &i_part)==2)
{
   c= r_part+ i_part*I;
    /* code */
}

else{
fprintf(stderr, "Error: Invalid input for -c. Expected r_part, i_part\n");
exit(EXIT_FAILURE);

}
break;

case 'o':
output_filename= optarg;
break;


case 'C':
color=true;
break;

case 't':
run_test=true;
break;

case 'h':
print_help();
exit(EXIT_SUCCESS);


default:
print_help();
exit(EXIT_SUCCESS);
    }


    size_t abs_height= (height<0) ? (size_t)-height : (size_t) height;
    size_t row_stride= color ? ((width*3+3)& ~3) : ((width+3) & ~3);
    unsigned char* img = malloc(row_stride* abs_height) ;

    /* code */
}

    julia_erdeniz(c, start, width, height, res, n, color, img);

    return 0;
}