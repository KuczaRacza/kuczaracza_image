#include "image.h"
#include "types.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
image *encode(bitmap *raw) {
  image *img = (image *)malloc(sizeof(image));
  img->format = raw->format;
  img->size_x = raw->x;
  img->size_y = raw->y;
#warning writes to read only pointer
  cubic_quantization(raw, 50, 0);
  img->length = 1;
  switch (img->format) {
  case RGB24:
    img->parts = malloc(sizeof(part24_rgb));
    break;
  case RGBA32:
    // img->parts = malloc(sizeof(part32_rgba));
    break;
  }
  img->parts->x = 0;
  img->parts->y = 0;
  img->parts->w = raw->x;
  img->parts->h = raw->y;
  switch (img->format) {
  case RGB24:
    img->parts->map = pallete_8rgb(raw, &img->parts->dict);
    break;
  case RGBA32:
    // img->parts->map = pallete_8rgba(raw, &img->parts->dict);
    break;
  }
  return img;
}
bitmap *decode(image *img) {
  bitmap *map = depallete_8rgb(img->parts[0].map, &img->parts->dict);
  return map;
}
stream seralize(image *img) {
  stream str;
  str.size = sizeof(u32) +  // size_x
             sizeof(u32) +  // size_y
             sizeof(u8) +   // format
             sizeof(u32) +  // lenght
             (sizeof(u16) + // x part
              sizeof(u16) + // y part
              sizeof(u16) + // w part
              sizeof(u16) + // h part
              sizeof(u8) +  // depth
              sizeof(u32) + // bitamp x
              sizeof(u32) + // bitamp y
              sizeof(u32) + // bitmap row
              sizeof(u8)    // bitamp format
              ) * img->length;
  u32 dict_size;
  switch (img->format) {
  case RGB24:
    dict_size = sizeof(dict8_rgb);
    break;

  case RGBA32:
    dict_size = sizeof(dict8_rgba);
    break;
  }
  str.size += dict_size * img->length;
  for (u32 i = 0; i < img->length; i++) {
    str.size += img->parts[i].w * img->parts[i].h * format_bpp(img->parts[i].map->format);
  }
  str.ptr = malloc(str.size);
  u32 offset = 0;
  dstoffsetcopy(str.ptr, &img->size_x, &offset, sizeof(u32));
  dstoffsetcopy(str.ptr, &img->size_y, &offset, sizeof(u32));
  dstoffsetcopy(str.ptr, &img->format, &offset, sizeof(u8));
  dstoffsetcopy(str.ptr, &img->length, &offset, sizeof(u32));
  for (u32 i = 0; i < img->length; i++) {
    dstoffsetcopy(str.ptr, &img->parts[i].x, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].y, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].w, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].h, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].depth, &offset, sizeof(u8));
    dstoffsetcopy(str.ptr, &img->parts[i].dict, &offset, dict_size);
    dstoffsetcopy(str.ptr, &img->parts[i].map->x, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, &img->parts[i].map->y, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, &img->parts[i].map->row, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, &img->parts[i].map->format, &offset, sizeof(u8));
    dstoffsetcopy(str.ptr, img->parts[i].map->ptr, &offset,
                  img->parts[i].w * img->parts[i].h * format_bpp(img->parts[i].map->format));
  }
  return str;
}
image *deserialize(stream str) {
  image *img = (image *)malloc(sizeof(image));
  u64 offset = 0;
  // header
  memcpy(img, (u8 *)str.ptr + offset, 3 * sizeof(u32) + sizeof(u8));
  offset += 3 * sizeof(u32) + sizeof(u8);
  // parts
  u32 part_length;
  u8 format;
  switch (img->format) {
  case RGB24:
    part_length = sizeof(dict8_rgb) + 4 * sizeof(u16) + sizeof(u8);
    img->parts = malloc(sizeof(part24_rgb));
    format = DICT8RGB;
    break;

  case RGBA32:
    part_length = sizeof(dict8_rgba) + 4 * sizeof(u16) + sizeof(u8);
    img->parts = malloc(sizeof(part32_rgba));
    format = DICT8RGBA;
    break;
  }

  for (u32 i = 0; i < img->length; i++) {
    memcpy(&img->parts[i], (u8 *)str.ptr + offset, part_length);
    offset += part_length;
    img->parts[i].map = create_bitmap(img->parts[i].w, img->parts[i].h,
                                      img->parts[i].w, format);
    offset += sizeof(u32) * 3 + sizeof(u8);
    u32 map_size = img->parts[i].w * img->parts[i].h * format_bpp(format);
    offset += sizeof(u32) * 3 + sizeof(u8);
    memcpy(img->parts[i].map->ptr, (u8 *)str.ptr + offset, map_size);
    offset += map_size;
  }
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
      u32 dcol = get_dict8rgb(color, d);
      if (dcol == 193) {

      } else {
        set_pixel(i, j, replacment, dcol);
      }
    }
  }
  return replacment;
}
u16 add_color_8rgb(dict8_rgb *d, u32 color) {

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
    d->size = 0;
    return 193;
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
      set_pixel(i, j, replacment, add_color_8rgba(d, color));
    }
  }
  return replacment;
}
bitmap *depallete_8rgba(bitmap *bit, dict8_rgba *d) {
  bitmap *replacment = create_bitmap(bit->x, bit->y, bit->row, RGBA32);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      u32 dcol = get_dict8rgba(color, d);
      if (dcol == 256) {
        // free replacment
        // return NULL;
      } else {
        set_pixel(i, j, replacment, dcol);
      }
    }
  }
  return replacment;
}
u16 add_color_8rgba(dict8_rgba *d, u32 color) {

  for (u16 i = 0; i < d->size; i++) {
    if (d->colors[i] == color) {
      return i;
    }
  }
  if (d->size == 255) {
    d->size = 0;
    return 256;
  }

  d->colors[d->size] = color;
  d->size++;
  return d->size - 1;
}
u32 get_dict8rgba(u8 index, dict8_rgba *d) { return d->colors[index]; }
void linear_quantization(bitmap *b, u32 quant, u8 alpha) {
  for (u32 i = 0; i < b->x; i++) {
    for (u32 j = 0; j < b->y; j++) {
      u32 color = get_pixel(i, j, b);
      u8 *col = (u8 *)&color;
      for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {

        // works better with floats
        col[k] = (col[k] / quant) * quant;
      }
      set_pixel(i, j, b, color);
    }
  }
}
void cubic_quantization(bitmap *b, u32 quant, u8 alpha) {
  for (u32 i = 0; i < b->x; i++) {
    for (u32 j = 0; j < b->y; j++) {
      u32 color = get_pixel(i, j, b);
      u8 *col = (u8 *)&color;
      float average = 0.0f;
      for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {
        float dynamic_quant = (float)quant + (col[k] * 0.05f);
        col[k] = round(col[k] / dynamic_quant) * floor(dynamic_quant);
      }
      set_pixel(i, j, b, color);
    }
  }
}
void rectangle_tree(image *img, bitmap *raw) {
  /*
  u32 depth = 0;
  switch (img->format) {
  case RGBA32:
    while (1) {

    }
    break;
  case RGB24:
    break;
  }
  */
}
