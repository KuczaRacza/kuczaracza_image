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
  linear_quantization(copy, 5, 0);
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
      rect part_rect;
      part_rect.x = img->parts[i].x;
      part_rect.y = img->parts[i].y;
      part_rect.w = img->parts[i].w;
      part_rect.h = img->parts[i].h;
      u32 quad = (img->max_depth - img->parts[i].depth) / (float)img->max_depth * 16.0f;
      quad = (quad < 4) ? 4 : quad;
      bitmap *btmp = recreate_quads(str, quad, 20, part_rect, img->format);
      for (u32 j = 0; j < img->parts[i].w; j++) {
        for (u32 k = 0; k < img->parts[i].h; k++) {
          u32 color = get_pixel(j, k, btmp);
          set_pixel(j + img->parts[i].x, k + img->parts[i].y, map, color);
        }
      }
    }
  }

  return map;
}
stream seralize(image *img) {
  stream str;
  str.size = sizeof(u32) +  // size_x
             sizeof(u32) +  // size_y
             sizeof(u8) +   // format
             sizeof(u8) +   // max depth
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
  dstoffsetcopy(str.ptr, &img->max_depth, &offset, sizeof(u8));
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
  srcoffsetcopy(&img->max_depth, str.ptr, &offset, sizeof(u8));
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
  d->size = 0;
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
  u8 max_depth = 0;
  for (u32 i = 0; i < length; i++) {
    u8 td = *(u8 *)(vec.data + elemnet_size * i + sizeof(rect));
    if (td > max_depth) {
      max_depth = td;
    }
  }
  if (max_depth == 0) {
    max_depth = 1;
  }
  img->max_depth = max_depth;
  for (u32 i = 0; i < length; i++) {
    rect trt = *(rect *)(vec.data + elemnet_size * i);
    u8 td = *(u8 *)(vec.data + elemnet_size * i + sizeof(rect));
    img->parts[i].x = trt.x;
    img->parts[i].y = trt.y;
    img->parts[i].w = trt.w;
    img->parts[i].h = trt.h;
    img->parts[i].depth = td;
    bitmap *btmp = copy_bitmap(raw, trt.x, trt.y, trt.w, trt.h);
    stream stmp;
    u32 quad = (max_depth - td) / (float)max_depth * 16.0f;
    quad = (quad < 4) ? 4 : quad;
    stmp = cut_quads(btmp, quad, 20);

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
      arect.w = area.w / 2 ;
      arect.h = area.h;
      brect.x = area.x + area.w / 2;
      brect.y = area.y;
      brect.w = area.w - area.w / 2;
      brect.h = area.h;

    } else {
      arect.x = area.x;
      arect.y = area.y;
      arect.w = area.w;
      arect.h = area.h / 2 ;
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
rgba_color color_diffrence(u32 color_b, u32 color_a) {
  rgba_color *a = (rgba_color *)&color_a;
  rgba_color *b = (rgba_color *)&color_b;
  rgba_color res;
  res.r = (a->r > b->r) ? a->r - b->r : b->r - a->r;
  res.g = (a->g > b->g) ? a->g - b->g : b->g - a->g;
  res.b = (a->b > b->b) ? a->b - b->b : b->b - a->b;
  res.r = (a->a > b->a) ? a->a - b->a : b->a - a->a;
  return res;
}
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold) {
  stream ret;
  u32 esize = format_bpp(b->format);
  ret.size = b->x * b->y * esize;
  ret.ptr = malloc(ret.size);
  u32 offset = 0;
  for (u32 i = 0; i < b->x / quad_s; i++) {
    for (u32 j = 0; j < b->y / quad_s; j++) {
      u32 cr[4];
      for (u32 k = 0; k < 4; k++) {
        cr[k] = 0;
      }
      cr[0] = get_pixel(i * quad_s, j * quad_s, b);
      cr[1] = get_pixel(i * quad_s + quad_s - 1, j * quad_s, b);
      cr[2] = get_pixel(i * quad_s + quad_s - 1, j * quad_s + quad_s - 1, b);
      cr[3] = get_pixel(i * quad_s, j * quad_s + quad_s - 1, b);
      dstoffsetcopy(ret.ptr, &cr[0], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[1], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[2], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[3], &offset, esize);
      u32 diffrence = 0;
      for (u32 k = 0; k < 4; k++) {
        for (u32 l = 0; l < 4; l++) {
          if (k != l) {
            rgba_color dif = color_diffrence(cr[k], cr[l]);
            if (diffrence < dif.r) {
              diffrence = dif.r;
            }
            if (diffrence < dif.g) {
              diffrence = dif.g;
            }
            if (diffrence < dif.b) {
              diffrence = dif.b;
            }
            if (esize == 4)
              if (diffrence < dif.a) {
                diffrence = dif.a;
              }
          }
        }
      }
      if (diffrence > threshold) {
        for (u32 k = 0; k < quad_s; k++) {
          for (u32 l = 0; l < quad_s; l++) {

            if (!(k == 0 && l == 0) && !(k == quad_s - 1 && l == quad_s - 1) &&
                !(k == 0 && l == quad_s - 1) && !(k == quad_s - 1 && l == 0)) {
              u32 pixel = get_pixel(i * quad_s + k, j * quad_s + l, b);
              dstoffsetcopy(ret.ptr, &pixel, &offset, esize);
            }
          }
        }
      }
    }
  }

  for (u32 i = 0; i < b->x; i++) {
    for (u32 j = 0; j < b->y; j++) {
      if (i >= (b->x / quad_s) * quad_s || j >= (b->y / quad_s) * quad_s) {
        u32 pixel = get_pixel(i, j, b);
        dstoffsetcopy(ret.ptr, &pixel, &offset, esize);
      }
    }
  }
  ret.size = offset;
  ret.ptr = realloc(ret.ptr, offset);
  return ret;
}
bitmap *recreate_quads(stream str, u8 quad_s, u8 threshold, rect size,
                       u8 format) {

  bitmap *ret = create_bitmap(size.w, size.h, size.w, format);
  u32 esize = format_bpp(format);
  str.ptr = realloc(str.ptr, size.w * size.h * esize);
  u32 offset = 0;
  for (u32 i = 0; i < ret->x / quad_s; i++) {
    for (u32 j = 0; j < ret->y / quad_s; j++) {
      u32 cr[4];
      for (u32 k = 0; k < 4; k++) {
        cr[k] = 0;
      }
      srcoffsetcopy(&cr[0], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[1], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[2], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[3], str.ptr, &offset, esize);
      set_pixel(i * quad_s, j * quad_s, ret, cr[0]);
      set_pixel(i * quad_s + quad_s - 1, j * quad_s, ret, cr[1]);
      set_pixel(i * quad_s + quad_s - 1, j * quad_s + quad_s - 1, ret, cr[2]);
      set_pixel(i * quad_s, j * quad_s + quad_s - 1, ret, cr[3]);
      u32 diffrence = 0;
      for (u32 k = 0; k < 4; k++) {
        for (u32 l = 0; l < 4; l++) {
          if (k != l) {
            rgba_color dif = color_diffrence(cr[k], cr[l]);
            if (diffrence < dif.r) {
              diffrence = dif.r;
            }
            if (diffrence < dif.g) {
              diffrence = dif.g;
            }
            if (diffrence < dif.b) {
              diffrence = dif.b;
            }
            if (esize == 4)
              if (diffrence < dif.a) {
                diffrence = dif.a;
              }
          }
        }
      }
      if (diffrence > threshold) {
        for (u32 k = 0; k < quad_s; k++) {
          for (u32 l = 0; l < quad_s; l++) {
            if (!(k == 0 && l == 0) && !(k == quad_s - 1 && l == quad_s - 1) &&
                !(k == 0 && l == quad_s - 1) && !(k == quad_s - 1 && l == 0)) {
              u32 pixel = 0;
              srcoffsetcopy(&pixel, str.ptr, &offset, esize);
              set_pixel(i * quad_s + k, j * quad_s + l, ret, pixel);
            }
          }
        }
      } else {
        rgba_color *corners = (rgba_color *)cr;
        for (u32 k = 1; k < quad_s - 1; k++) {
          float dist = (float)k / (quad_s - 1);
          rgba_color pixel;
          rgba_color pixelb = *(rgba_color *)&cr[0];
          rgba_color pixela = *(rgba_color *)&cr[1];
          pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
          pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
          pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
          if (esize == 4) {
            pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
          }
          set_pixel(quad_s * i + k, quad_s * j, ret, *(u32 *)&pixel);
        }
        for (u32 k = 1; k < quad_s - 1; k++) {
          float dist = (float)k / (quad_s - 1);
          rgba_color pixel;
          rgba_color pixelb = *(rgba_color *)&cr[3];
          rgba_color pixela = *(rgba_color *)&cr[2];
          pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
          pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
          pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
          if (esize == 4) {
            pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
          }
          set_pixel(quad_s * i + k, quad_s * j + quad_s - 1, ret,
                    *(u32 *)&pixel);
        }
        for (u32 k = 0; k < quad_s; k++) {
          u32 cola = get_pixel(quad_s * i + k, quad_s * j, ret);
          u32 colb = get_pixel(quad_s * i + k, quad_s * j + quad_s - 1, ret);
          rgba_color pixelb = *(rgba_color *)&cola;
          rgba_color pixela = *(rgba_color *)&colb;
          for (u32 l = 1; l < quad_s - 1; l++) {

            float dist = (float)l / (quad_s - 1);
            rgba_color pixel;

            pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
            pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
            pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
            if (esize == 4) {
              pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
            }
            set_pixel(quad_s * i + k, quad_s * j + l, ret, *(u32 *)&pixel);
          }
        }
      }
    }
  }

  for (u32 i = 0; i < ret->x; i++) {
    for (u32 j = 0; j < ret->y; j++) {
      if (i >= (ret->x / quad_s) * quad_s || j >= (ret->y / quad_s) * quad_s) {
        u32 pixel = 0;
        srcoffsetcopy(&pixel, str.ptr, &offset, esize);
        set_pixel(i, j, ret, pixel);
      }
    }
  }
  free(str.ptr);
  return ret;
}
