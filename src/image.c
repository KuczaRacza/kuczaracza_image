#include "image.h"
#include "types.h"
#include <SDL2/SDL_pixels.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
image *encode(bitmap *raw) {
  image *img = (image *)malloc(sizeof(image));
  img->format = raw->format;
  img->size_x = raw->x;
  img->size_y = raw->y;
  bitmap *copy = copy_bitmap(raw, 0, 0, raw->x, raw->y);
  linear_quantization(copy, 15, 0);
  rectangle_tree(img, copy);

  return img;
}
bitmap *decode(image *img) {

  bitmap *map =
      create_bitmap(img->size_x, img->size_y, img->size_x, img->format);
  if (img->format == RGB24) {

    for (u32 i = 0; i < img->length; i++) {
      bitmap *tmp = depallete_8rgb(img->parts[i].map, &img->parts[i].dict);
      part24_rgb *prt = &img->parts[i];
      // not optimalized
      for (u32 j = 0; j < tmp->x; j++) {
        for (u32 k = 0; k < tmp->y; k++) {
          u32 color = get_pixel(j, k, tmp);
          set_pixel(j + img->parts[i].x, k + img->parts[i].y, map, color);
        }
      }
      free_bitmap(tmp);
    }
  }

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
              sizeof(u32) + // bitmap rowcd
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
    str.size += img->parts[i].w * img->parts[i].h *
                format_bpp(img->parts[i].map->format);
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
                  img->parts[i].w * img->parts[i].h *
                      format_bpp(img->parts[i].map->format));
  }
  return str;
}
image *deserialize(stream str) {
  image *img = (image *)malloc(sizeof(image));
  u32 offset = 0;
  srcoffsetcopy(&img->size_x, str.ptr, &offset, sizeof(u32));
  srcoffsetcopy(&img->size_y, str.ptr, &offset, sizeof(u32));
  srcoffsetcopy(&img->format, str.ptr, &offset, sizeof(u8));
  srcoffsetcopy(&img->length, str.ptr, &offset, sizeof(u32));
  u32 dict_size;
  switch (img->format) {
  case RGB24:
    dict_size = sizeof(dict8_rgb);
    img->parts = malloc(sizeof(part24_rgb) * img->length);
    break;

  case RGBA32:
    dict_size = sizeof(dict8_rgba);
    img->parts = malloc(sizeof(part32_rgba) * img->length);
    break;
  default:
    fprintf(stderr, "wrong pixelformat: %i\n", (i32)img->format);
    return NULL;
  }
  for (u32 i = 0; i < img->length; i++) {

    srcoffsetcopy(&img->parts[i].x, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].y, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].w, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].h, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].depth, str.ptr, &offset, sizeof(u8));
    srcoffsetcopy(&img->parts[i].dict, str.ptr, &offset, dict_size);
    u32 xb;
    u32 yb;
    u32 rb;
    u8 fb;
    srcoffsetcopy(&xb, str.ptr, &offset, sizeof(u32));
    srcoffsetcopy(&yb, str.ptr, &offset, sizeof(u32));
    srcoffsetcopy(&rb, str.ptr, &offset, sizeof(u32));
    srcoffsetcopy(&fb, str.ptr, &offset, sizeof(u8));
    img->parts[i].map = create_bitmap(xb, yb, rb, fb);
    srcoffsetcopy(img->parts[i].map->ptr, str.ptr, &offset,
                  xb * yb * format_bpp(fb));
  }

  return img;
}
bitmap *pallete_8rgb(bitmap *bit, dict8_rgb *dict) {
  dict->size = 0;
  bitmap *replacement = create_bitmap(bit->x, bit->y, bit->x, DICT8RGB);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      u32 dcol = add_color_8rgb(dict, color);
      if (dcol == 193) {
        set_pixel(i, j, replacement, 1);
      } else {
        set_pixel(i, j, replacement, dcol);
      }
    }
  }

  return replacement;
}
bitmap *depallete_8rgb(bitmap *bit, dict8_rgb *d) {
  bitmap *replacment = create_bitmap(bit->x, bit->y, bit->row, RGB24);
  for (u32 i = 0; i < bit->x; i++) {
    for (u32 j = 0; j < bit->y; j++) {
      u32 color = get_pixel(i, j, bit);
      u32 dcol = get_dict8rgb(color, d);

      set_pixel(i, j, replacment, dcol);
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
  if (d->size == 255) {
    return 256;
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
  d->size = 0;
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
  u32 parts_no = 1;
  u8 ok = 0;
  while (ok != 1) {
    ok = 1;
    for (u32 i = 0; i < parts_no; i++) {
      float part_size = (float)raw->x / parts_no;
      u32 colornum = count_colors_rect(
          raw, roundf(part_size * i), 0,
          roundf(part_size * (i + 1)) - roundf(part_size * i), raw->x);
      if (colornum > 255) {
        ok = 0;
        parts_no *= 2;
        break;
      }
    }
  }

  img->parts = malloc(sizeof(part24_rgb) * parts_no);
  float part_size = (float)raw->x / parts_no;
  img->length = parts_no;
  for (u32 i = 0; i < parts_no; i++) {
    bitmap *tmp = copy_bitmap(
        raw, roundf(part_size * i), 0,
        roundf(part_size * (i + 1)) - roundf(part_size * i), raw->y);
    img->parts[i].map = pallete_8rgb(tmp, &img->parts[i].dict);
    img->parts[i].depth = 0;
    img->parts[i].x = roundf(part_size * i);
    img->parts[i].y = 0;
    img->parts[i].w = roundf(part_size * (i + 1)) - roundf(part_size * i) -1;
    img->parts[i].h = raw->y;
    free_bitmap(tmp);
  }
}

u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h) {
  u32 colors[256];
  u32 number = 0;
  for (u32 i = x; i < x + w; i++) {
    for (u32 j = y; j < y + h; j++) {
      u32 pixel = get_pixel(i, j, b);
      u8 unique = 1;
      for (u32 k = 0; k < number; k++) {
        if (colors[k] == pixel) {
          unique = 0;
          break;
        }
      }
      if (unique == 1) {
        if (number == 255) {
          return 256;
        } else {
          colors[number] = pixel;
          number++;
        }
      }
    }
  }
  return number;
}
u32 count_colors(bitmap *b) { return count_colors_rect(b, 0, 0, b->x, b->y); }