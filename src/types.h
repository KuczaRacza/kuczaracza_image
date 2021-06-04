#pragma once
#include <bits/stdint-uintn.h>
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
enum pixel_format  { RGBA32, RGB24, DICT8, DICT16 };
typedef enum pixel_format pixel_format;
struct bitmap {
  u32 x;
  u32 y;
  u8 format;
  u8 *ptr;
};
typedef struct bitmap bitmap;
bitmap *create_bitmap(u32 size_x, u32 size_y, u8 fmt);
u32 get_pixel(u32 x, u32 y, bitmap *map);
u8 format_bpp(u8 fmt);
void set_pixel(u32 x, u32 y, bitmap *map, u32 color);
