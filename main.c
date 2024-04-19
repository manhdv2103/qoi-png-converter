#include "lodepng.h"
#include "qoi.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("missing input file operand\n");
    return 1;
  }

  if (argc < 3) {
    printf("missing destination file operand\n");
    return 1;
  }

  char *input_file = argv[1];
  char *dest_file = argv[2];

  unsigned error;
  unsigned char *image = 0;
  unsigned width, height;

  error = lodepng_decode32_file(&image, &width, &height, input_file);
  if (error) {
    printf("error: %u - %s\n", error, lodepng_error_text(error));
    return 1;
  }

  error = qoi_encode_file(dest_file, image, width, height, QOI_SRGB);
  if (error) {
    printf("error: %u\n", error);
    return 1;
  }

  free(image);

  return 0;
}
