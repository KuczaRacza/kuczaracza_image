#include "image.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
image *encode(bitmap *raw) {
  image *img = (image *)malloc(sizeof(image));
  img->comp = 0;
  img->size_x = raw->x;
  img->size_y = raw->y;
  img->pixels = *raw;
  img->pixels.ptr = malloc(raw->x * raw->y * format_bpp(raw->format));
  memcpy(img->pixels.ptr, raw->ptr, raw->x * raw->y * format_bpp(raw->format));
  return img;
}
bitmap *decode(image *img) {
  bitmap *map = create_bitmap(img->pixels.x, img->pixels.y, img->pixels.format);
  memcpy(map->ptr, img->pixels.ptr, map->x * map->y * format_bpp(map->format));
  return map;
}
stream seralize(image *img) {
  stream str;
  str.size = sizeof(u32) + sizeof(u32) + sizeof(u8) + sizeof(u32) +
             sizeof(u32) + sizeof(u8) +
             (img->pixels.x * img->pixels.y * format_bpp(img->pixels.format));
  str.ptr = (u8 *)malloc(str.size);
  u64 offset = 0;
  // comping header
  memcpy((u8 *)str.ptr + offset, img, 2 * sizeof(u32) + sizeof(u8));
  offset += 2 * sizeof(u32) + sizeof(u8);
  // coping bitmap header
  memcpy((u8 *)str.ptr + offset, &img->pixels, 2 * sizeof(u32) + sizeof(u8));
  offset += 2 * sizeof(u32) + sizeof(u8);
  // coping bitmap
  memcpy((u8 *)str.ptr + offset, img->pixels.ptr,
         img->pixels.x * img->pixels.y * format_bpp(img->pixels.format));
  return str;
}
image *deserialize(stream str) {
  image *img = (image *)malloc(sizeof(image));
  u64 offset = 0;
  memcpy(img, (u8 *)str.ptr + offset, 2 * sizeof(u32) + sizeof(u8));
  offset += sizeof(u32) * 2 + sizeof(u8);
  memcpy(&img->pixels, (u8 *)str.ptr + offset, 2 * sizeof(u32) + sizeof(u8));
  offset += sizeof(u32) * 2 + sizeof(u8);
  u64 data_size =
      img->pixels.x * img->pixels.y * format_bpp(img->pixels.format);
  img->pixels.ptr = (u8 *)malloc(data_size);
  memcpy(img->pixels.ptr, (u8 *)str.ptr + offset, data_size);
  return img;
}
