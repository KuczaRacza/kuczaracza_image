#include "image.h"
#include "types.h"
#include <SDL2/SDL_pixels.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
image *encode(bitmap *raw) {
  // alocating struct
  image *img = (image *)malloc(sizeof(image));
  // setting basic informations
  img->format = raw->format;
  img->size_x = raw->x;
  img->size_y = raw->y;
  // coping bitmap to avoid overwriting input
  bitmap *copy = copy_bitmap(raw, 0, 0, raw->x, raw->y);
  // reducing clors
  linear_quantization(copy, 5, 0);
  // commpression
  rectangle_tree(img, copy);
  free_bitmap(copy);

  return img;
}
bitmap *decode(image *img) {
  // creating output bitmap
  bitmap *map =
      create_bitmap(img->size_x, img->size_y, img->size_x, img->format);

  u32 esize = format_bpp(img->format);
  for (u32 i = 0; i < img->length; i++) {
    // translating from pallete to real colors
    stream str = depallete(img->parts[i].pixels, &img->parts[i].dict, esize);
    // area of current part
    rect part_rect;
    part_rect.x = img->parts[i].x;
    part_rect.y = img->parts[i].y;
    part_rect.w = img->parts[i].w;
    part_rect.h = img->parts[i].h;
    // detrmining quad blokcs size
    u32 quad =
        (img->max_depth - img->parts[i].depth) / (float)img->max_depth * 25.0f;
    quad = (quad < 6) ? 6 : quad;
    // color diffrence limit after which quad is no longer commpressed
    u32 threshold = 25 + img->parts[i].depth / (float)img->max_depth * 15.0f;
    // interoplating quads
    bitmap *btmp = recreate_quads(str, quad, threshold, part_rect, img->format);
    // coping pixels
    for (u32 j = 0; j < img->parts[i].w; j++) {
      for (u32 k = 0; k < img->parts[i].h; k++) {
        u32 color = get_pixel(j, k, btmp);
        set_pixel(j + img->parts[i].x, k + img->parts[i].y, map, color);
      }
    }
    free_bitmap(btmp);
  }

  return map;
}
stream seralize(image *img) {
  stream str;
  u32 esize = format_bpp(img->format);
  str.size = sizeof(u32) +  // size_x
             sizeof(u32) +  // size_y
             sizeof(u8) +   // format
             sizeof(u8) +   // max depth
             sizeof(u32) +  // lenght
                            // foreach part of image
             (sizeof(u16) + // x part
              sizeof(u16) + // y part
              sizeof(u16) + // w part
              sizeof(u16) + // h part
              sizeof(u8) +  // depth
              sizeof(u32) + // stream size
              sizeof(u8)    // dict size
              ) * img->length;
  // calulating sie of pallets and pixels
  for (u32 i = 0; i < img->length; i++) {
    str.size += img->parts[i].pixels.size;
    str.size += img->parts[i].dict.size * sizeof(esize);
  }
  str.ptr = malloc(str.size);
  u32 offset = 0;
  dstoffsetcopy(str.ptr, &img->size_x, &offset, sizeof(u32));
  dstoffsetcopy(str.ptr, &img->size_y, &offset, sizeof(u32));
  dstoffsetcopy(str.ptr, &img->format, &offset, sizeof(u8));
  dstoffsetcopy(str.ptr, &img->max_depth, &offset, sizeof(u8));
  dstoffsetcopy(str.ptr, &img->length, &offset, sizeof(u32));
  // coping each part
  for (u32 i = 0; i < img->length; i++) {
    dstoffsetcopy(str.ptr, &img->parts[i].x, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].y, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].w, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].h, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].depth, &offset, sizeof(u8));
    dstoffsetcopy(str.ptr, &img->parts[i].dict.size, &offset, sizeof(u8));
    // coping dict with  stripping unesseary data
    for (u32 j = 0; j < img->parts[i].dict.size; j++) {
      dstoffsetcopy(str.ptr, &img->parts[i].dict.colors[j], &offset, esize);
    }

    dstoffsetcopy(str.ptr, &img->parts[i].pixels.size, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, img->parts[i].pixels.ptr, &offset,
                  img->parts[i].pixels.size);
  }
  return str;
}
image *deserialize(stream str) {
  image *img = (image *)malloc(sizeof(image));
  u32 offset = 0;
  // coping basic info about image
  srcoffsetcopy(&img->size_x, str.ptr, &offset, sizeof(u32));
  srcoffsetcopy(&img->size_y, str.ptr, &offset, sizeof(u32));
  srcoffsetcopy(&img->format, str.ptr, &offset, sizeof(u8));
  srcoffsetcopy(&img->max_depth, str.ptr, &offset, sizeof(u8));
  srcoffsetcopy(&img->length, str.ptr, &offset, sizeof(u32));
  // size of each pixel
  u32 esize = format_bpp(img->format);
  img->parts = malloc(sizeof(part24_rgb) * img->length);
  for (u32 i = 0; i < img->length; i++) {

    srcoffsetcopy(&img->parts[i].x, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].y, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].w, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].h, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].depth, str.ptr, &offset, sizeof(u8));
    srcoffsetcopy(&img->parts[i].dict.size, str.ptr, &offset, sizeof(u8));
    // coping pallete and aligning to better performnce
    for (u32 j = 0; j < img->parts[i].dict.size; j++) {
      srcoffsetcopy(&img->parts[i].dict.colors[j], str.ptr, &offset, esize);
    }
    // coping pixels
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
      // omits alpha if not present in image
      for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {
        col[k] = roundf(col[k] / (float)quant) * (float)quant;
      }
      set_pixel(i, j, b, color);
    }
  }
}

stream pallete(stream str, dict8_rgb *d, u32 esize) {
  stream ret;
  // reseting pallet size
  d->size = 0;
  // calulating size of pixel after palleting
  ret.size = str.size / esize;
  ret.ptr = malloc(ret.size);
  for (u32 i = 0; i < ret.size; i++) {
    u32 pixel;
    // coping each pixel in loop
    memcpy(&pixel, (u8 *)str.ptr + (i * esize), esize);
    // tring to add to pallete
    u32 dcol = add_color(d, pixel);
    if (dcol == 256) {
      // overflow
      ret.ptr[i] = 0;
    } else {
      // writning down new palleted  color
      ret.ptr[i] = (u8)dcol;
    }
  }
  return ret;
}
stream depallete(stream str, dict8_rgb *d, u32 esize) {
  stream ret;
  // calulating new size of deplleted pixels
  ret.size = str.size * esize;
  ret.ptr = malloc(ret.size);
  for (u32 i = 0; i < str.size; i++) {
    // lookup in pallete;
    u32 dcol = str.ptr[i];
    u32 pixel = get_dict(dcol, d);
    // writing down
    memcpy((u8 *)ret.ptr + (esize * i), &pixel, esize);
  }
  return ret;
}
u16 add_color(dict8_rgb *d, u32 color) {

  for (u16 i = 0; i < d->size; i++) {
    // cheking if color is already in pallete
    if (d->colors[i] == color) {
      return i;
    }
  }
  // overflow
  if (d->size == 255) {
    return 256;
  }
  // addding color and returning new entery
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
  // vector of rects
  vector vec;
  vec.size = 0;
  vec.capacity = 40;
  vec.data = malloc(40);
  // dimensions of image
  rect start_area;
  start_area.x = 0;
  start_area.y = 0;
  start_area.w = raw->x;
  start_area.h = raw->y;
  // recurensive function that write to vec rect of every part
  create_rect(&vec, raw, start_area, 0);
  // size of elemnet in vec
  u32 elemnet_size = sizeof(rect) + sizeof(u8);
  // vector size
  u32 length = vec.size / elemnet_size;
  img->length = length;
  // allocating space needed to write down parts
  img->parts = malloc(sizeof(part24_rgb) * length);
  u8 max_depth = 0;
  // determing max depth of rectangular tree
  for (u32 i = 0; i < length; i++) {
    // reading depth in each entery in vec
    u8 td = *(u8 *)(vec.data + elemnet_size * i + sizeof(rect));
    if (td > max_depth) {
      max_depth = td;
    }
  }
  // to avoid dividing by 0
  if (max_depth == 0) {
    max_depth = 1;
  }
  img->max_depth = max_depth;
  // encoding each part
  for (u32 i = 0; i < length; i++) {
    // reading rect of rectangles
    rect trt = *(rect *)(vec.data + elemnet_size * i);
    u8 td = *(u8 *)(vec.data + elemnet_size * i + sizeof(rect));
    // setting rects
    img->parts[i].x = trt.x;
    img->parts[i].y = trt.y;
    img->parts[i].w = trt.w;
    img->parts[i].h = trt.h;
    img->parts[i].depth = td;
    bitmap *btmp = copy_bitmap(raw, trt.x, trt.y, trt.w, trt.h);
    stream stmp;
    // size of each quad in cuttnig qads
    u32 quad = (max_depth - td) / (float)max_depth * 25.0f;
    quad = (quad < 6) ? 6 : quad;
    u32 threshold = 25 + td / (float)max_depth * 15.0f;
    // pixels with cutted  unnesseary quads
    stmp = cut_quads(btmp, quad, threshold);
    // palletting pixels and witing to image struct
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
        // cheking for repating colors
        if (colors[k] == pixel) {
          unique = 0;
          break;
        }
      }
      if (unique == 1) {
        colors[number] = pixel;
        // in case of raching limit
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
  // counting colors
  u32 cols = count_colors_rect(raw, area.x, area.y, area.w, area.h);
  // if limit is reached
  if (cols > 255) {
    // cereating two rects
    rect arect;
    rect brect;
    // if depth is even splits horizontal if odd splits vertical
    if (depth % 2) {

      arect.x = area.x;
      arect.y = area.y;
      arect.w = area.w / 2;
      arect.h = area.h;
      brect.x = area.x + area.w / 2;
      brect.y = area.y;
      brect.w = area.w - area.w / 2;
      brect.h = area.h;

    } else {
      arect.x = area.x;
      arect.y = area.y;
      arect.w = area.w;
      arect.h = area.h / 2;
      brect.x = area.x;
      brect.y = area.y + area.h / 2;
      brect.w = area.w;
      brect.h = area.h - area.h / 2;
    }
    // for splitted area checks if another splis are nessesary
    create_rect(rects, raw, arect, depth + 1);
    create_rect(rects, raw, brect, depth + 1);
  } else {
    // if color limit is not exceed writes down area to vector
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
  res.a = (a->a > b->a) ? a->a - b->a : b->a - a->a;
  return res;
}
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold) {
  // pixels are transalted form bitamp into array where some blocks are skipped
  stream ret;
  u32 esize = format_bpp(b->format);
  ret.size = b->x * b->y * esize;
  ret.ptr = malloc(ret.size);
  u32 offset = 0;
  // for each block
  for (u32 i = 0; i < b->x / quad_s; i++) {
    for (u32 j = 0; j < b->y / quad_s; j++) {
      // corners
      u32 cr[4];
      for (u32 k = 0; k < 4; k++) {
        cr[k] = 0;
      }
      // gets colors of corners
      cr[0] = get_pixel(i * quad_s, j * quad_s, b);
      cr[1] = get_pixel(i * quad_s + quad_s - 1, j * quad_s, b);
      cr[2] = get_pixel(i * quad_s + quad_s - 1, j * quad_s + quad_s - 1, b);
      cr[3] = get_pixel(i * quad_s, j * quad_s + quad_s - 1, b);
      // writing down corners
      dstoffsetcopy(ret.ptr, &cr[0], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[1], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[2], &offset, esize);
      dstoffsetcopy(ret.ptr, &cr[3], &offset, esize);
      // caluclates biggest diffrence in color per channel
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
      // if  diffrence is bigger that means that block cannot be skipped
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
  // writes part of rectangle thath is on edges and cannot be encoded to quads
  for (u32 i = 0; i < b->x; i++) {
    for (u32 j = 0; j < b->y; j++) {
      if (i >= (b->x / quad_s) * quad_s || j >= (b->y / quad_s) * quad_s) {
        u32 pixel = get_pixel(i, j, b);
        dstoffsetcopy(ret.ptr, &pixel, &offset, esize);
      }
    }
  }
  ret.size = offset;
  // resize  array to adjust to stripped data
  ret.ptr = realloc(ret.ptr, offset);
  return ret;
}
bitmap *recreate_quads(stream str, u8 quad_s, u8 threshold, rect size,
                       u8 format) {
  // translate form array to bitmap  and interpolate missing quds
  bitmap *ret = create_bitmap(size.w, size.h, size.w, format);
  // size of pixel
  u32 esize = format_bpp(format);
  str.ptr = realloc(str.ptr, size.w * size.h * esize);
  u32 offset = 0;
  for (u32 i = 0; i < ret->x / quad_s; i++) {
    for (u32 j = 0; j < ret->y / quad_s; j++) {
      u32 cr[4];
      for (u32 k = 0; k < 4; k++) {
        cr[k] = 0;
      }
      // coping corners first
      srcoffsetcopy(&cr[0], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[1], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[2], str.ptr, &offset, esize);
      srcoffsetcopy(&cr[3], str.ptr, &offset, esize);
      set_pixel(i * quad_s, j * quad_s, ret, cr[0]);
      set_pixel(i * quad_s + quad_s - 1, j * quad_s, ret, cr[1]);
      set_pixel(i * quad_s + quad_s - 1, j * quad_s + quad_s - 1, ret, cr[2]);
      set_pixel(i * quad_s, j * quad_s + quad_s - 1, ret, cr[3]);
      u32 diffrence = 0;
      // determining if diffrence is lesser or greater tahn thershold

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
            // block is not wasn't skipped and  other pixels are  copied
            if (!(k == 0 && l == 0) && !(k == quad_s - 1 && l == quad_s - 1) &&
                !(k == 0 && l == quad_s - 1) && !(k == quad_s - 1 && l == 0)) {
              u32 pixel = 0;
              srcoffsetcopy(&pixel, str.ptr, &offset, esize);
              set_pixel(i * quad_s + k, j * quad_s + l, ret, pixel);
            }
          }
        }
      } else {
        // block was skipped and   pixels are interpolated
        rgba_color *corners = (rgba_color *)cr;
        for (u32 k = 1; k < quad_s - 1; k++) {
          // intrpolating horizontally   upper edge
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
        // intrpolating horizontally   lower edge
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
        // interpolating vertically each column
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
void free_image(image *img) {
  for (u32 i = 0; i < img->length; i++) {
    free(img->parts[i].pixels.ptr);
  }
  free(img->parts);
  free(img);
}