#include "image.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
image *encode(bitmap *raw) {
  image *img = (image *)malloc(sizeof(image));
  img->format = raw->format;
  img->size_x = raw->x;
  img->size_y = raw->y;
  img->pixels = pallete_8rgba(raw, &img->dict);
  // = create_bitmap(raw->x, raw->y, raw->row, raw->format);
  // memcpy(img->pixels->ptr,raw->ptr, img->pixels->y * img->pixels->row *
  // format_bpp(img->pixels->format));
  return img;
}
bitmap *decode(image *img) {
  bitmap *map = depallete_8rgba(img->pixels, &img->dict);
  return map;
}
stream seralize(image *img) {
  stream str;
  str.size =
      sizeof(u32) + sizeof(u32) + sizeof(u8) + sizeof(u32) + sizeof(u32) +
      sizeof(u8) + sizeof(u32) + sizeof(dict8_rgba) +
      (img->pixels->row * img->pixels->y * format_bpp(img->pixels->format));
  str.ptr = (u8 *)malloc(str.size);
  u64 offset = 0;
  // comping header
  memcpy((u8 *)str.ptr + offset, img, 2 * sizeof(u32) + sizeof(u8) + sizeof(dict8_rgba));
  offset += 2 * sizeof(u32) + sizeof(u8) + sizeof(dict8_rgba);
  // coping bitmap header
  memcpy((u8 *)str.ptr + offset, img->pixels, 3 * sizeof(u32) + sizeof(u8));
  offset += 3 * sizeof(u32) + sizeof(u8);
  // coping bitmap
  memcpy((u8 *)str.ptr + offset, img->pixels->ptr,
         img->pixels->row * img->pixels->y * format_bpp(img->pixels->format));
  return str;
}
image *deserialize(stream str) {
  image *img = (image *)malloc(sizeof(image));
  u64 offset = 0;
  // copying header
  // add dict
  memcpy(img, (u8 *)str.ptr + offset, 2 * sizeof(u32) + sizeof(u8) + sizeof(dict8_rgba));
  offset += sizeof(u32) * 2 + sizeof(u8) + sizeof(dict8_rgba);
  // coping bitmap header
  bitmap tmp;
  memcpy(&tmp, (u8 *)str.ptr + offset, sizeof(u32) * 3 + sizeof(u8));

  img->pixels = create_bitmap(tmp.x, tmp.y, tmp.row, tmp.format);

  memcpy(img->pixels, (u8 *)str.ptr + offset, 3 * sizeof(u32) + sizeof(u8));
  offset += sizeof(u32) * 3 + sizeof(u8);
  // coping bitmap
  u64 data_size =
      img->pixels->y * img->pixels->row * format_bpp(img->pixels->format);
  memcpy(img->pixels->ptr, (u8 *)str.ptr + offset, data_size);
  return img;
}
bitmap *pallete_8rgb(bitmap *bit, dict8_rgb *dict) {
  dict->size = 0;
  bitmap *replacement = create_bitmap(bit->x, bit->y, bit->x, DICT8RGB);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      set_pixel(i, j, replacement, add_color_8rgb(dict, color));
    }
  }
#warning memory leak
  return replacement;
}
bitmap *depallete_8rgb(bitmap *bit, dict8_rgb *d) {
  bitmap *replacment = create_bitmap(bit->x, bit->y, bit->row, RGB24);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      set_pixel(i, j, replacment, get_dict8rgb(color, d));
    }
  }
  return replacment;
}
u8 add_color_8rgb(dict8_rgb *d, u32 color) {

  u8 *address = (u8 *)d->colors;
  for (u16 i = 0; i < d->size; i++) {
    u32 dcol = 0;
    memcpy(&dcol, address, 3);
    address = (u8 *)address + 3;
    if (dcol == color) {
      return i;
    }
  }
  if (d->size == 192) {
    return 0;
  }
  address = (u8 *)d->colors + (d->size * 3);
  memcpy(address, &color, 3);
  d->size++;

  return d->size - 1;
}
u32 get_dict8rgb(u8 index, dict8_rgb *d) {
  u8 *address = (u8 *)d->colors + index * 3;
  u32 color = 0;
  memcpy(&color, address, 3);
  return color;
}
bitmap *pallete_8rgba(bitmap *bit, dict8_rgba *d) {
  bitmap *replacment = create_bitmap(bit->x, bit->y, bit->row, RGBA32);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      set_pixel(i, j, replacment,add_color_8rgba(d, color));
    }
  }
  return replacment;
}
bitmap *depallete_8rgba(bitmap *bit, dict8_rgba *d) {
  bitmap *replacment = create_bitmap(bit->x, bit->y, bit->row, RGBA32);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      set_pixel(i, j, replacment, get_dict8rgba(color, d));
    }
  }
  return replacment;
}
u8 add_color_8rgba(dict8_rgba *d, u32 color) {

  for (u16 i = 0; i < d->size; i++) {
    if (d->colors[i] == color) {
      return i;
    }
  }
  if (d->size == 255) {
    return 0;
  }

  d->colors[d->size] = color;
  d->size++;
  return d->size - 1;
}
u32 get_dict8rgba(u8 index, dict8_rgba *d) { return d->colors[index]; }
