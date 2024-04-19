#ifndef QOI_H
#define QOI_H

#include <stdint.h>

#define QOI_HEADER_MAGIC                                                       \
  { 'q', 'o', 'i', 'f' }

#define QOI_END_MARKER                                                         \
  {                                                                            \
    (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0,    \
        (uint8_t)0, (uint8_t)1                                                 \
  }

typedef enum QoiChannel {
  QOI_RGB = 3,
  QOI_RGBA = 4,
} QoiChannel;

typedef enum QoiColorspace {
  QOI_SRGB = 0,
  QOI_LINEAR = 1,
} QoiColorspace;

#pragma pack(push)
#pragma pack(1)

typedef struct QoiHeader {
  char magic[4];      // magic bytes "qoif"
  uint32_t width;     // image width in pixels (BE)
  uint32_t height;    // image height in pixels (BE)
  uint8_t channels;   // 3 = RGB, 4 = RGBA
  uint8_t colorspace; // 0 = sRGB with linear alpha
                      // 1 = all channels linear
} QoiHeader;

#pragma pack(pop)

typedef struct QoiPixel {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} QoiPixelValue;

// unsigned int qoi_decode_file(unsigned char **out, unsigned int *w,
//                              unsigned int *h, const char *filename);

// Encode image to qoi image file
unsigned int qoi_encode_file(const char *filename, const unsigned char *image,
                             unsigned int w, unsigned int h,
                             QoiColorspace colorspace);

typedef enum QoiErrorCode {
  QOI_NO_ERROR,
  QOI_MEMORY_ALLOCATION_ERROR,
  QOI_FILE_WRITING_ERROR,
} QoiErrorCode;

// const char *qoi_error_text(unsigned int code);

#endif
