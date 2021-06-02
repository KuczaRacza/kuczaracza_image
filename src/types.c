#include "types.h"
#include <stdlib.h>
bitmap *create_bitmap(u32 size_x, u32 size_y, pixel_format fmt) {
  u8 bpp = format_bpp(fmt);
  u32 map_size = size_x * size_y * bpp;
  bitmap *b = (bitmap *)malloc(sizeof(bitmap) + map_size);
  b->format = fmt;
  b->x = size_x;
  b->y = size_y;
  b->ptr = (u8 *)b + sizeof(bitmap);
  return b;
}
u8 format_bpp(pixel_format fmt) {
  u8 bpp;
  switch (fmt) {
  case RGBA32:
    bpp = 4;
    break;
  case RGB24:
    bpp = 3;
    break;
  case DICT16:
    bpp = 2;
    break;
  case DICT8:
    bpp = 1;
    break;
  }
  return bpp;
}
u32 get_pixel(u32 x, u32 y, bitmap *map) {
  u8 bpp = format_bpp(map->format);
  u8 *pixel_address = map->ptr + (bpp * (y * map->x + x));
  u32 pixel;
  switch (bpp) {
  case 4:
    pixel = *(u32 *)pixel_address;
    break;
  case 3:
    pixel = *(u32 *)pixel_address & 0xFFFFFF00;
    break;
  case 2:
    pixel = *(u16 *)pixel_address;
    break;
  case 1:
    pixel = *pixel_address;
    break;
  };
  return pixel;
}
void set_pixel(u32 x, u32 y, bitmap *map, u32 color) {
  u8 bpp = format_bpp(map->format);
  u8 *pixel_address = map->ptr + (bpp * (y * map->x + x));
  switch (bpp) {
  case 4:
    *(u32 *)pixel_address = color;
    break;
  case 3:
    *(u32 *)pixel_address = *(u32 *)pixel_address | (color & 0xFFFFFF00);
    break;
  case 2:
    *(u16 *)pixel_address = (u16)color;
    break;
  case 1:
    *pixel_address = (u8)color;
    break;
  };
};