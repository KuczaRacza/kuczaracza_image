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
  linear_quantization(copy, 6, 0);
  rectangle_tree(img, copy);

  return img;
}
bitmap *decode(image *img) {

  bitmap *map =
      create_bitmap(img->size_x, img->size_y, img->size_x, img->format);
  if (img->format == RGB24) {
    u32 esize = format_bpp(img->format);
    for (u32 i = 0; i < img->length; i++) {
      stream str = depallete(img->parts[i].pixels, &img->parts[i].dict, esize);
      // not optimalized
	  
      for (u32 j = 0; j < img->parts[i].w; j++) {
        for (u32 k = 0; k < img->parts[i].h; k++) {
          u32 color = 0;
          memcpy(&color, (u8 *)str.ptr + ((k* img->parts[i].w + j ) * esize), esize);
          set_pixel(j + img->parts[i].x, k + img->parts[i].y, map, color);
        }
      }
	  free(str.ptr);
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
              sizeof(u32)   // stream size
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
    str.size += img->parts[i].pixels.size;
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
    dstoffsetcopy(str.ptr, &img->parts[i].pixels.size, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, img->parts[i].pixels.ptr, &offset,
                  img->parts[i].pixels.size);
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
    u32 ssize = 0;
    srcoffsetcopy(&ssize, str.ptr, &offset, sizeof(u32));
    img->parts[i].pixels.size = ssize;
    img->parts[i].pixels.ptr = malloc(ssize);
    srcoffsetcopy(img->parts[i].pixels.ptr, str.ptr, &offset, ssize);
  }

  return img;
}
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

stream pallete(stream str, dict8_rgb *d, u32 esize) {
  stream ret;
  d->size =0;
  ret.size = str.size / esize;
  ret.ptr = malloc(ret.size);
  for (u32 i = 0; i < ret.size; i++) {
    u32 pixel;
    memcpy(&pixel, (u8 *)str.ptr + (i * esize), esize);
    u32 dcol = add_color(d, pixel);
    if (dcol == 256) {
      ret.ptr[i] = 0;
    } else {
      ret.ptr[i] = (u8)dcol;
    }
  }
  return ret;
}
stream depallete(stream str, dict8_rgb *d, u32 esize) {
  stream ret;
  ret.size = str.size * esize;
  ret.ptr = malloc(ret.size);
  for (u32 i = 0; i < str.size; i++) {
    u32 dcol = str.ptr[i];
    if (dcol < 255) {
      u32 pixel = get_dict(dcol, d);
      memcpy((u8 *)ret.ptr + (esize * i), &pixel, esize);
    }
  }
  return ret;
}
u16 add_color(dict8_rgb *d, u32 color) {
	
  for (u16 i = 0; i < d->size; i++) {
    if (d->colors[i] == color) {
      return i;
    }
  }
  if (d->size == 255) {
    return 256;
  }

  d->colors[d->size] = color;
  d->size++;
  return d->size - 1;
}
u32 get_dict(u8 index, dict8_rgb *d) { return d->colors[index]; }
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
  vector vec;
  vec.size = 0;
  vec.capacity = 40;
  vec.data = malloc(40);
  rect start_area;
  start_area.x = 0;
  start_area.y = 0;
  start_area.w = raw->x;
  start_area.h = raw->y;
  create_rect(&vec, raw, start_area, 0);
  u32 elemnet_size = sizeof(rect) + sizeof(u8);
  u32 length = vec.size / elemnet_size;
  img->length = length;
  img->parts = malloc(sizeof(part24_rgb) * length);
  for (u32 i = 0; i < length; i++) {
    rect trt = *(rect *)(vec.data + elemnet_size * i);
    u8 td = *(u8 *)(vec.data + elemnet_size * i + sizeof(rect));
    img->parts[i].x = trt.x;
    img->parts[i].y = trt.y;
    img->parts[i].w = trt.w;
    img->parts[i].h = trt.h;
    bitmap *btmp = copy_bitmap(raw, trt.x, trt.y, trt.w, trt.h);
    stream stmp;
    stmp.size = btmp->row * btmp->y * format_bpp(btmp->format);
    stmp.ptr = malloc(stmp.size);
    memcpy(stmp.ptr, btmp->ptr, stmp.size);

    img->parts[i].pixels =
        pallete(stmp, &img->parts[i].dict, format_bpp(btmp->format));

    free_bitmap(btmp);
    free(stmp.ptr);
  }
  free(vec.data);
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
        colors[number] = pixel;
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
void create_rect(vector *rects, bitmap *raw, rect area, u8 depth) {
  u32 cols = count_colors_rect(raw, area.x, area.y, area.w, area.h);
  if (cols > 255) {
    rect arect;
    rect brect;

    if (depth % 2) {

      arect.x = area.x;
      arect.y = area.y;
      arect.w = area.w / 2 - 1;
      arect.h = area.h;
      brect.x = area.x + area.w / 2;
      brect.y = area.y;
      brect.w = area.w - area.w / 2;
      brect.h = area.h;

    } else {
      arect.x = area.x;
      arect.y = area.y;
      arect.w = area.w;
      arect.h = area.h / 2 - 1;
      brect.x = area.x;
      brect.y = area.y + area.h / 2;
      brect.w = area.w;
      brect.h = area.h - area.h / 2;
    }
    create_rect(rects, raw, arect, depth + 1);
    create_rect(rects, raw, brect, depth + 1);
  } else {
    push_vector(rects, &area, sizeof(area));
    push_vector(rects, &depth, sizeof(u8));
  }
}