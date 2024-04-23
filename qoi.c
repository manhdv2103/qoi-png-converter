#include "qoi.h"
#include "stdbool.h"
#include <arpa/inet.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUN_CHUNK_BIAS -1
#define DIFF_CHUNK_BIAS 2
#define LUMA_CHUNK_DG_BIAS 32
#define LUMA_CHUNK_DR_DB_BIAS 8
#define MAX_RUN_LENGTH 62
#define STATIC_TMP_DATA_SIZE 128

typedef enum QoiChunkTag {
  RGB_TAG = 0xfe,
  RGBA_TAG = 0xff,
  INDEX_TAG = 0x00, // only use the first 2 bits
  DIFF_TAG = 0x40,  // only use the first 2 bits
  LUMA_TAG = 0x80,  // only use the first 2 bits
  RUN_TAG = 0xc0,   // only use the first 2 bits
  END_TAG = 0x01,
} QoiChunkTag;

QoiHeader create_header(uint32_t width, uint32_t height,
                        QoiColorspace colorspace) {
  QoiHeader header = {.magic = QOI_HEADER_MAGIC,
                      .width = htonl(width),
                      .height = htonl(height),
                      .channels = (uint8_t)QOI_RGBA,
                      .colorspace = (uint8_t)colorspace};
  return header;
}

unsigned int get_index_position(QoiPixelValue pixel) {
  return (pixel.red * 3 + pixel.green * 5 + pixel.blue * 7 + pixel.alpha * 11) %
         64;
}

bool is_pixel_equals(const QoiPixelValue a, const QoiPixelValue b) {
  return (a.red == b.red) && (a.green == b.green) && (a.blue == b.blue) &&
         (a.alpha == b.alpha);
}

const uint8_t qoi_end_marker[] = QOI_END_MARKER;

/* ////////////////////////////////////////////////////////////////////////// */

unsigned int qoi_encode_file(const char *filename, const unsigned char *image,
                             unsigned int w, unsigned int h,
                             QoiColorspace colorspace) {
  int result = QOI_NO_ERROR;
  QoiHeader header = create_header(w, h, colorspace);

  unsigned int buffer_size = 1024 + sizeof(header);
  unsigned int used_buffer_size = 0;
  unsigned char *data_buffer = (uint8_t *)malloc(buffer_size);
  if (data_buffer == NULL) {
    result = QOI_MEMORY_ALLOCATION_ERROR;
    goto end;
  }

  // Prepare space for the header later
  used_buffer_size += sizeof(header);

  QoiPixelValue seen_pixels[64] = {0};
  QoiPixelValue prev_pixel = {.red = 0, .green = 0, .blue = 0, .alpha = 255};
  seen_pixels[get_index_position(prev_pixel)] = prev_pixel;

  unsigned char static_tmp_data[STATIC_TMP_DATA_SIZE];
  uint64_t run_length = 0;
  bool contains_alpha = false;

  unsigned int data_length = w * h;
  for (unsigned long i = 0; i < data_length + 1; i++) {
    QoiPixelValue pixel = {
        .red = i == data_length ? 0 : image[i * 4],
        .green = i == data_length ? 0 : image[i * 4 + 1],
        .blue = i == data_length ? 0 : image[i * 4 + 2],
        .alpha = i == data_length ? 0 : image[i * 4 + 3],
    };

    unsigned char *new_data = static_tmp_data;
    bool is_new_data_from_heap = false;
    bool redo_pixel = false;
    uint8_t new_data_size = 0;

    if (i < data_length && is_pixel_equals(pixel, prev_pixel)) {
      run_length++;
      continue;
    }

    // Run chunk
    if (run_length > 0) {
      unsigned int chunk_num =
          ceil(((double)run_length) / ((double)MAX_RUN_LENGTH));
      unsigned int last_chunk_run_length = run_length % MAX_RUN_LENGTH;
      last_chunk_run_length =
          last_chunk_run_length == 0 ? MAX_RUN_LENGTH : last_chunk_run_length;
      new_data_size = chunk_num;

      if (chunk_num > STATIC_TMP_DATA_SIZE) {
        new_data = malloc(chunk_num);
        if (new_data == NULL) {
          result = QOI_MEMORY_ALLOCATION_ERROR;
          goto end;
        }

        is_new_data_from_heap = true;
      }

      for (unsigned int j = 0; j < chunk_num; j++) {
        new_data[j] = RUN_TAG | ((j == chunk_num - 1 ? last_chunk_run_length
                                                     : MAX_RUN_LENGTH) +
                                 RUN_CHUNK_BIAS);
      }

      run_length = 0;
      if (i < data_length)
        redo_pixel = true;
      goto packing_result;
    }

    // Index chunk
    QoiPixelValue seen_pixel = seen_pixels[get_index_position(pixel)];
    if (is_pixel_equals(pixel, seen_pixel)) {
      new_data[0] = INDEX_TAG | get_index_position(pixel);
      new_data_size = 1;
      goto packing_result;
    }

    // Rgba chunk
    new_data[0] = RGBA_TAG;
    new_data[1] = pixel.red;
    new_data[2] = pixel.green;
    new_data[3] = pixel.blue;
    new_data[4] = pixel.alpha;
    new_data_size = 5;

    if (prev_pixel.alpha == pixel.alpha) {
      uint8_t dr = pixel.red - prev_pixel.red;
      uint8_t dg = pixel.green - prev_pixel.green;
      uint8_t db = pixel.blue - prev_pixel.blue;

      // Diff chunk
      uint8_t bias_dr = dr + DIFF_CHUNK_BIAS;
      uint8_t bias_dg = dg + DIFF_CHUNK_BIAS;
      uint8_t bias_db = db + DIFF_CHUNK_BIAS;
      if (bias_dr < 4 && bias_dg < 4 && bias_db < 4) {
        new_data[0] = DIFF_TAG | (bias_dr << 4 | bias_dg << 2 | bias_db);
        new_data_size = 1;
        goto packing_result;
      }

      // Luma chunk
      bias_dg = dg + LUMA_CHUNK_DG_BIAS;
      uint8_t bias_dr_dg = dr - dg + LUMA_CHUNK_DR_DB_BIAS;
      uint8_t bias_db_dg = db - dg + LUMA_CHUNK_DR_DB_BIAS;
      if (bias_dg < 64 && bias_dr_dg < 16 && bias_db_dg < 16) {
        new_data[0] = LUMA_TAG | bias_dg;
        new_data[1] = bias_dr_dg << 4 | bias_db_dg;
        new_data_size = 2;
        goto packing_result;
      }

      // Rgb chunk
      new_data[0] = RGB_TAG;
      new_data_size = 4;
    }

  packing_result:
    if (used_buffer_size + new_data_size > buffer_size) {
      buffer_size *= 2;
      unsigned char *new_buffer =
          (unsigned char *)realloc(data_buffer, buffer_size);
      if (new_buffer == NULL) {
        result = QOI_MEMORY_ALLOCATION_ERROR;
        goto end;
      }
      data_buffer = new_buffer;
    }

    memcpy(data_buffer + used_buffer_size, new_data, new_data_size);
    used_buffer_size += new_data_size;
    if (is_new_data_from_heap)
      free(new_data);

    if (redo_pixel) {
      i--;
    } else {
      if (pixel.alpha < UCHAR_MAX)
        contains_alpha = true;
      seen_pixels[get_index_position(pixel)] = pixel;
      prev_pixel = pixel;
    }
  }

  if (!contains_alpha)
    header.channels = (uint8_t)QOI_RGB;
  memcpy(data_buffer, &header, sizeof(header));

  memcpy(data_buffer + used_buffer_size, qoi_end_marker,
         sizeof(qoi_end_marker));
  used_buffer_size += sizeof(qoi_end_marker);

  FILE *fptr = fopen(filename, "wb");
  unsigned long nmemb = fwrite(data_buffer, used_buffer_size, 1, fptr);
  int close_res = fclose(fptr);
  if (nmemb != 1 || close_res != 0) {
    result = QOI_FILE_WRITING_ERROR;
  }

end:
  free(data_buffer);
  return result;
}
