#pragma once
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
struct stream {
  u8 *ptr;
  u64 size;
};
typedef struct stream stream;
enum pixel_format { RGBA32, RGB24, DICTRGBA,DICTRGB,YUV444  };
typedef enum pixel_format pixel_format;
struct bitmap {
  u32 x;
  u32 y;
  u32 row;
  u8 format;
  u8 *ptr;
};
typedef struct bitmap bitmap;
struct rect {
  u32 x;
  u32 y;
  u32 w;
  u32 h;
};
typedef struct rect rect;
struct vector {
  u32 size;
  u32 capacity;
  u8 *data;
};
typedef struct vector vector;
bitmap *create_bitmap(u32 size_x, u32 size_y, u32 row, u8 fmt);
bitmap *copy_bitmap(bitmap *orgin, u32 x, u32 y, u32 w, u32 h);
u32 get_pixel(u32 x, u32 y, bitmap *map);
u8 format_bpp(u8 fmt);
void set_pixel(u32 x, u32 y, bitmap *map, u32 color);
void free_bitmap(bitmap *b);
void dstoffsetcopy(void *dst, void *src, u32 *offset, u32 size);
void srcoffsetcopy(void *dst, void *src, u32 *offset, u32 size);
void push_vector(vector *vec, void *data, u32 size);
u8 *get_element_vector(vector *vec, u32 index, u32 size);
