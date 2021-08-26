#include "image.h"
#include "types.h"
#include <SDL2/SDL_pixels.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>
#define SHOW_BLOCKS 1
image *encode(bitmap *raw, u32 max_block_size, u32 color_reduction,
              u32 block_color_sensivity, u32 complexity) {
  // alocating struct
  image *img = (image *)malloc(sizeof(image));
  // setting basic informations
  img->format = raw->format;
  img->size_x = raw->x;
  img->size_y = raw->y;
  img->blok_size = max_block_size;
  img->color_quant = color_reduction;
  img->color_sensivity = block_color_sensivity;
  // coping bitmap to avoid overwriting input
  bitmap *copy = copy_bitmap(raw, 0, 0, raw->x, raw->y);
  // reducing clors
  if (color_reduction > 1) {
    linear_quantization(copy, color_reduction, 0);
  }
  // commpression
  rectangle_tree(img, copy, max_block_size, block_color_sensivity, complexity);
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
    stream str = depallete(img->parts[i].pixels,
                           &img->dicts[img->parts[i].dict_index], esize);
    // area of current part
    rect part_rect;
    part_rect.x = img->parts[i].x;
    part_rect.y = img->parts[i].y;
    part_rect.w = img->parts[i].w;
    part_rect.h = img->parts[i].h;
    // detrmining quad blokcs size
    u32 quad =
        powf((img->max_depth - img->parts[i].depth) / (float)img->max_depth,
             3.0f) *

        img->blok_size;
    quad = (quad < 6) ? 6 : quad;
    // interoplating quads
    bitmap *btmp =
        recreate_quads(str, quad, part_rect, img->format, img->parts[i].blokcs);
    // coping pixels
    for (u32 j = 0; j < img->parts[i].h; j++) {
      for (u32 k = 0; k < img->parts[i].w; k++) {
        u32 color = get_pixel(k, j, btmp);
        set_pixel(k + img->parts[i].x, j + img->parts[i].y, map, color);
      }
    }
    free_bitmap(btmp);
  }

  return map;
}
stream seralize(image *img) {
  stream str;
  u32 esize = format_bpp(img->format);
  str.size = sizeof(u16) +   // size_x
             sizeof(u16) +   // size_y
             sizeof(u8) +    // format
             sizeof(u8) +    // max depth
             sizeof(u16) +   // block size
             sizeof(u16) +   // quant
             sizeof(u16) +   // block sensivity
             sizeof(u32) +   // lenght
             sizeof(u32) +   // dict lenght
                             // foreach part of image
             (sizeof(u16) +  // x part
              sizeof(u16) +  // y part
              sizeof(u16) +  // w part
              sizeof(u16) +  // h part
              sizeof(u8) +   // depth
              sizeof(u32) +  // stream size
              sizeof(u32) +  // block size
              sizeof(u16)) * // dict index
                 img->length;
  // calulating sie of pallets and pixels
  for (u32 i = 0; i < img->length; i++) {
    str.size += img->parts[i].pixels.size;
    str.size += img->parts[i].blokcs.size;
  }
  str.size += img->dicts_length * sizeof(u8);
  for (u32 i = 0; i < img->dicts_length; i++) {
    str.size += img->dicts[i].size * esize;
  }
  str.ptr = malloc(str.size);
  u32 offset = 0;
  dstoffsetcopy(str.ptr, &img->size_x, &offset, sizeof(u16));
  dstoffsetcopy(str.ptr, &img->size_y, &offset, sizeof(u16));
  dstoffsetcopy(str.ptr, &img->format, &offset, sizeof(u8));
  dstoffsetcopy(str.ptr, &img->max_depth, &offset, sizeof(u8));
  dstoffsetcopy(str.ptr, &img->blok_size, &offset, sizeof(u16));
  dstoffsetcopy(str.ptr, &img->color_quant, &offset, sizeof(u16));
  dstoffsetcopy(str.ptr, &img->color_sensivity, &offset, sizeof(u16));
  dstoffsetcopy(str.ptr, &img->length, &offset, sizeof(u32));
  dstoffsetcopy(str.ptr, &img->dicts_length, &offset, sizeof(u32));
  // coping each part
  for (u32 i = 0; i < img->length; i++) {
    dstoffsetcopy(str.ptr, &img->parts[i].x, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].y, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].w, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].h, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].depth, &offset, sizeof(u8));
    dstoffsetcopy(str.ptr, &img->parts[i].dict_index, &offset, sizeof(u16));
    dstoffsetcopy(str.ptr, &img->parts[i].pixels.size, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, img->parts[i].pixels.ptr, &offset,
                  img->parts[i].pixels.size);
    dstoffsetcopy(str.ptr, &img->parts[i].blokcs.size, &offset, sizeof(u32));
    dstoffsetcopy(str.ptr, img->parts[i].blokcs.ptr, &offset,
                  img->parts[i].blokcs.size);
  }
  for (u32 i = 0; i < img->dicts_length; i++) {
    dstoffsetcopy(str.ptr, &img->dicts[i].size, &offset, sizeof(u8));
    for (u32 j = 0; j < img->dicts[i].size; j++) {
      dstoffsetcopy(str.ptr, &img->dicts[i].colors[j], &offset, esize);
    }
  }
  stream compressed_str;
  compressed_str.size = str.size;
  compressed_str.ptr = malloc(compressed_str.size);
  compressed_str.size = ZSTD_compress(compressed_str.ptr, compressed_str.size,
                                      str.ptr, str.size, 22);
  free(str.ptr);
  compressed_str.ptr =
      realloc(compressed_str.ptr, compressed_str.size + sizeof(u64));
  memcpy(compressed_str.ptr + compressed_str.size, &str.size, sizeof(u64));
  compressed_str.size += sizeof(u64);
  return compressed_str;
}
image *deserialize(stream compressed_str) {
  stream str;
  memcpy(&str.size, compressed_str.ptr + compressed_str.size - sizeof(u64),
         sizeof(u64));
  ;
  str.ptr = malloc(str.size);
  str.size = ZSTD_decompress(str.ptr, str.size, compressed_str.ptr,
                             compressed_str.size - sizeof(u64));
  str.ptr = realloc(str.ptr, str.size);
  image *img = (image *)malloc(sizeof(image));
  u32 offset = 0;
  // coping basic info about image
  srcoffsetcopy(&img->size_x, str.ptr, &offset, sizeof(u16));
  srcoffsetcopy(&img->size_y, str.ptr, &offset, sizeof(u16));
  srcoffsetcopy(&img->format, str.ptr, &offset, sizeof(u8));
  srcoffsetcopy(&img->max_depth, str.ptr, &offset, sizeof(u8));
  srcoffsetcopy(&img->blok_size, str.ptr, &offset, sizeof(u16));
  srcoffsetcopy(&img->color_quant, str.ptr, &offset, sizeof(u16));
  srcoffsetcopy(&img->color_sensivity, str.ptr, &offset, sizeof(u16));
  srcoffsetcopy(&img->length, str.ptr, &offset, sizeof(u32));
  srcoffsetcopy(&img->dicts_length, str.ptr, &offset, sizeof(u32));
  // size of each pixel
  u32 esize = format_bpp(img->format);
  img->parts = malloc(sizeof(region) * img->length);
  for (u32 i = 0; i < img->length; i++) {

    srcoffsetcopy(&img->parts[i].x, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].y, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].w, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].h, str.ptr, &offset, sizeof(u16));
    srcoffsetcopy(&img->parts[i].depth, str.ptr, &offset, sizeof(u8));
    srcoffsetcopy(&img->parts[i].dict_index, str.ptr, &offset, sizeof(u16));
    // coping pixels
    u32 ssize = 0;
    srcoffsetcopy(&ssize, str.ptr, &offset, sizeof(u32));
    img->parts[i].pixels.size = ssize;
    img->parts[i].pixels.ptr = malloc(ssize);
    srcoffsetcopy(img->parts[i].pixels.ptr, str.ptr, &offset, ssize);
    u32 bsize = 0;
    srcoffsetcopy(&bsize, str.ptr, &offset, sizeof(u32));
    img->parts[i].blokcs.size = bsize;
    img->parts[i].blokcs.ptr = malloc(bsize);
    srcoffsetcopy(img->parts[i].blokcs.ptr, str.ptr, &offset, bsize);
  }
  img->dicts = calloc(sizeof(dict8), img->dicts_length);
  for (u32 i = 0; i < img->dicts_length; i++) {
    srcoffsetcopy(&img->dicts[i].size, str.ptr, &offset, sizeof(u8));
    for (u32 j = 0; j < img->dicts[i].size; j++) {
      srcoffsetcopy(&img->dicts[i].colors[j], str.ptr, &offset, esize);
    }
  }

  return img;
}
void linear_quantization(bitmap *b, u32 quant, u8 alpha) {
  for (u32 i = 0; i < b->y; i++) {
    for (u32 j = 0; j < b->x; j++) {
      u32 color = get_pixel(j, i, b);
      u8 *col = (u8 *)&color;
      float q = quant;
      // omits alpha if not present in image
      for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {
        float cl = roundf((float)col[k] / q) * (float)q;
        col[k] = (cl > 255) ? 255 : cl;
      }
      set_pixel(j, i, b, color);
    }
  }
}

stream pallete(stream str, dict8 *d, u32 esize) {
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
stream depallete(stream str, dict8 *d, u32 esize) {
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
u16 add_color(dict8 *d, u32 color) {

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
u32 get_dict(u8 index, dict8 *d) { return d->colors[index]; }
void cubic_quantization(bitmap *b, u32 quant, u8 alpha) {
  for (u32 i = 0; i < b->y; i++) {
    for (u32 j = 0; j < b->x; j++) {
      u32 color = get_pixel(j, i, b);
      u8 *col = (u8 *)&color;
      float average = 0.0f;
      for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {
        float dynamic_quant = (float)quant + (col[k] * 0.05f);
        col[k] = round(col[k] / dynamic_quant) * floor(dynamic_quant);
      }
      set_pixel(j, i, b, color);
    }
  }
}
void rectangle_tree(image *img, bitmap *raw, u32 max_block_size,
                    u32 block_color_sensivity, u32 complexity) {
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
  img->dicts_length = length;
  // allocating space needed to write down parts
  img->parts = malloc(sizeof(region) * length);
  img->dicts = calloc(sizeof(dict8), length);

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
  u32 dict_len = 0;
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
    u32 quad = powf((max_depth - td) / (float)max_depth, 3.0f) * max_block_size;
    quad = (quad < 6) ? 6 : quad;
    u32 threshold = block_color_sensivity +
                    (1.0f - td / (float)max_depth) * block_color_sensivity;
    // alocates array where algorithms foreach blocks are written
    u32 blsize = (trt.w / quad) * (trt.h / quad);
    if (blsize % 4 != 0) {
      blsize /= 4;
      blsize++;
    } else {
      blsize /= 4;
    }
    img->parts[i].blokcs.size = blsize;
    img->parts[i].blokcs.ptr = calloc(1, blsize);
    // pixels with cutted  unnesseary quads
    stmp = cut_quads(btmp, quad, threshold, img->parts[i].blokcs);
    // palletting pixels and witing to image struct

    img->parts[i].pixels =
        pallete(stmp, &img->dicts[dict_len], format_bpp(btmp->format));
    if (dict_len > 0 && complexity > 0) {
      u8 merged = 0;
      u32 max_dict_tries;
      if (complexity == 2) {
        max_dict_tries = dict_len - (dict_len / 2);
      } else if (complexity == 1) {
        max_dict_tries = dict_len - 5;
      } else {
        max_dict_tries = 0;
      }
      for (i32 j = dict_len; j > max_dict_tries; j--) {
        if (img->dicts[dict_len - j].size + img->dicts[dict_len].size < 300) {
          if (merge_dicts(&img->dicts[dict_len - j], &img->dicts[dict_len],
                          &img->parts[i].pixels) == 1) {
            img->parts[i].dict_index = dict_len - j;
            merged = 1;
            break;
          }
        }
      }
      if (merged == 0) {
        img->parts[i].dict_index = dict_len;
        dict_len++;
      }

    } else {
      img->parts[i].dict_index = dict_len;
      dict_len++;
    }
    free_bitmap(btmp);
    free(stmp.ptr);
  }
  img->dicts_length = dict_len;
  img->dicts = realloc(img->dicts, sizeof(dict8) * dict_len);
  free(vec.data);
}

u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h) {
  u32 colors[128];
  u32 number = 0;
  for (u32 i = y; i < y + h; i++) {
    for (u32 j = x; j < x + w; j++) {
      u32 pixel = get_pixel(j, i, b);
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
        if (number == 127) {
          return 128;
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
  if (cols > 127) {
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
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold, stream blocks) {
  // pixels are transalted form bitamp into array where some blocks are skipped
  stream ret;
  u32 esize = format_bpp(b->format);
  ret.size = b->x * b->y * esize;
  ret.ptr = malloc(ret.size);
  u32 offset = 0;
  // for each block
  u32 x_blocks_size = b->x / quad_s;
  u32 y_blocks_size = b->y / quad_s;
  if (x_blocks_size == 0) {
    x_blocks_size = 1;
    quad_s = b->x;
  }
  if (y_blocks_size == 0) {
    y_blocks_size = 1;
    quad_s = b->y;
  }

  for (u32 i = 0; i < y_blocks_size; i++) {
    for (u32 j = 0; j < x_blocks_size; j++) {
      // corners

      u32 qx = quad_s;
      u32 qy = quad_s;

      if (i + 1 == y_blocks_size) {
        qy = b->y - quad_s * i;
      }
      if (j + 1 == b->x / quad_s) {
        qx = b->x - quad_s * j;
      }

      // algorithm  used in this block
      u8 algo;
      u64 diffrence_sum = 0;
      u32 min_channel[4];
      u32 max_channel[4];
      memset(min_channel, 0xFFFFFFFF, sizeof(u32) * 4);
      memset(max_channel, 0x0, sizeof(u32) * 4);
      for (u32 k = 0; k < qy; k++) {
        for (u32 l = 0; l < qx; l++) {
          u32 pixel = get_pixel(j * quad_s + l, i * quad_s + k, b);

          for (u32 m = 0; m < esize; m++) {
            u8 *value = (u8 *)(&pixel) + m;
            if (*value > max_channel[m]) {
              max_channel[m] = *value;
            }
            if (*value < min_channel[m]) {
              min_channel[m] = *value;
            }
          }
        }
      }
      u32 biggest_diffrence = 0;
      for (u32 m = 0; m < esize; m++) {
		  u32 this_diffrence =max_channel[m] - min_channel[m];
		  if(this_diffrence>biggest_diffrence){
			  biggest_diffrence = this_diffrence;
		  }
      }

      // caluclates biggest diffrence in color per channel
      
      // if  diffrence is bigger that means that block cannot be skipped
      if (biggest_diffrence >= threshold) {
        for (u32 k = 0; k < qy; k++) {
          for (u32 l = 0; l < qx; l++) {
            u32 pixel = get_pixel(j * quad_s + l, i * quad_s + k, b);
            dstoffsetcopy(ret.ptr, &pixel, &offset, esize);
          }
        }
        algo = 1;
      } else {
        u32 cr[4];
        memset(cr, 0, sizeof(u32) * 4);
        if (0) {
          cr[0] = get_pixel(j * quad_s, i * quad_s, b);
          cr[1] = get_pixel(j * quad_s + qx - 1, i * quad_s, b);
          cr[2] = get_pixel(j * quad_s + qx - 1, i * quad_s + qy - 1, b);
          cr[3] = get_pixel(j * quad_s, i * quad_s + qy - 1, b);
        } else {
          rect areas[4];
          areas[0].x = j * quad_s;
          areas[1].x = j * quad_s + qx - (qx / 4);
          areas[2].x = j * quad_s + qx - (qx / 4);
          areas[3].x = j * quad_s;
          areas[0].w = qx / 4 - 1;
          areas[1].w = qx / 4 - 1;
          areas[2].w = qx / 4 - 1;
          areas[3].w = qx / 4 - 1;
          areas[0].y = i * quad_s;
          areas[1].y = i * quad_s;
          areas[2].y = i * quad_s + qy - (qy / 4);
          areas[3].y = i * quad_s + qy - (qy / 4);
          areas[0].h = qy / 4 - 1;
          areas[1].h = qy / 4 - 1;
          areas[2].h = qy / 4 - 1;
          areas[3].h = qy / 4 - 1;
          for (u32 k = 0; k < 4; k++) {
            if (areas[k].w == 0) {
              areas[k].w = 1;
            }
            if (areas[k].h == 0) {
              areas[k].h = 1;
            }
          }
          cr[0] = average_color(areas[0], b);
          cr[1] = average_color(areas[1], b);
          cr[2] = average_color(areas[2], b);
          cr[3] = average_color(areas[3], b);
        }
        dstoffsetcopy(ret.ptr, &cr[0], &offset, esize);
        dstoffsetcopy(ret.ptr, &cr[1], &offset, esize);
        dstoffsetcopy(ret.ptr, &cr[2], &offset, esize);
        dstoffsetcopy(ret.ptr, &cr[3], &offset, esize);
        algo = 0;
      }
      u32 pos = i * (x_blocks_size) + j;
      blocks.ptr[pos / 4] = blocks.ptr[pos / 4] | (algo << ((pos % 4) * 2));
    }
  }

  ret.size = offset;
  // resize  array to adjust to stripped data
  ret.ptr = realloc(ret.ptr, offset);
  return ret;
}
bitmap *recreate_quads(stream str, u8 quad_s, rect size, u8 format,
                       stream blokcs) {
  // translate form array to bitmap  and interpolate missing quds
  bitmap *ret = create_bitmap(size.w, size.h, size.w, format);
  // size of pixel
  u32 esize = format_bpp(format);
  // realocating output stream to avoid memory safety issues if file is corupted
  str.ptr = realloc(str.ptr, size.w * size.h * esize);
  // index of  written last byte in stream
  u32 offset = 0;
  u32 x_blocks_size = ret->x / quad_s;
  u32 y_blocks_size = ret->y / quad_s;
  if (x_blocks_size == 0) {
    x_blocks_size = 1;
    quad_s = ret->x;
  }
  if (y_blocks_size == 0) {
    y_blocks_size = 1;
    quad_s = ret->y;
  }
  for (u32 i = 0; i < y_blocks_size; i++) {
    for (u32 j = 0; j < x_blocks_size; j++) {
      u32 cr[4];
      memset(cr, 0, sizeof(u32) * 4);
      u32 qx = quad_s;
      u32 qy = quad_s;

      if (i + 1 == ret->y / quad_s) {
        qy = ret->y - quad_s * i;
      }
      if (j + 1 == ret->x / quad_s) {
        qx = ret->x - quad_s * j;
      }

      // coping corners first

      // determining if diffrence is lesser or greater tahn thershold
      // algorimthm used in block

      u16 pos = i * x_blocks_size + j;
      u32 bitshift = (pos % 4 * 2);
      u8 algo = (blokcs.ptr[pos / 4] & (3 << bitshift)) >> bitshift;

      if (algo == 1) {

        for (u32 k = 0; k < qy; k++) {
          for (u32 l = 0; l < qx; l++) {
            // block is  wasn't skipped and  other pixels are  copied

            u32 pixel = 0;
            srcoffsetcopy(&pixel, str.ptr, &offset, esize);
#if SHOW_BLOCKS
            if (l == qx - 1 || k == qy - 1) {
              pixel = 0x000000FF;
            }
#endif
            set_pixel(j * quad_s + l, i * quad_s + k, ret, pixel);
          }
        }
      } else if (algo == 0) {

        srcoffsetcopy(&cr[0], str.ptr, &offset, esize);
        srcoffsetcopy(&cr[1], str.ptr, &offset, esize);
        srcoffsetcopy(&cr[2], str.ptr, &offset, esize);
        srcoffsetcopy(&cr[3], str.ptr, &offset, esize);
        set_pixel(j * quad_s, i * quad_s, ret, cr[0]);
        set_pixel(j * quad_s + qx - 1, i * quad_s, ret, cr[1]);
        set_pixel(j * quad_s + qx - 1, i * quad_s + qy - 1, ret, cr[2]);
        set_pixel(j * quad_s, i * quad_s + qy - 1, ret, cr[3]);
        // block was skipped and   pixels are interpolated
        rgba_color *corners = (rgba_color *)cr;
        for (u32 k = 1; k < qx - 1; k++) {
          // intrpolating horizontally   upper edge
          float dist = (float)k / (qx - 1);
          rgba_color pixel;
          rgba_color pixelb = *(rgba_color *)&cr[0];
          rgba_color pixela = *(rgba_color *)&cr[1];
          pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
          pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
          pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
          if (esize == 4) {
            pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
          }
          set_pixel(quad_s * j + k, quad_s * i, ret, *(u32 *)&pixel);
        }
        // intrpolating horizontally   lower edge
        for (u32 k = 1; k < qx - 1; k++) {
          float dist = (float)k / (qx - 1);
          rgba_color pixel;
          rgba_color pixelb = *(rgba_color *)&cr[3];
          rgba_color pixela = *(rgba_color *)&cr[2];
          pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
          pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
          pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
          if (esize == 4) {
            pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
          }
          set_pixel(quad_s * j + k, quad_s * i + qy - 1, ret, *(u32 *)&pixel);
        }
        // interpolating vertically each column
        for (u32 k = 0; k < qx; k++) {
          u32 cola = get_pixel(quad_s * j + k, quad_s * i, ret);
          u32 colb = get_pixel(quad_s * j + k, quad_s * i + qy - 1, ret);
          rgba_color pixelb = *(rgba_color *)&cola;
          rgba_color pixela = *(rgba_color *)&colb;
          for (u32 l = 1; l < qy - 1; l++) {

            float dist = (float)l / (qy - 1);
            rgba_color pixel;

            pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
            pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
            pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
            if (esize == 4) {
              pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
            }
            set_pixel(quad_s * j + k, quad_s * i + l, ret, *(u32 *)&pixel);
          }
        }
#if SHOW_BLOCKS
        for (u32 k = 0; k < qy; k++) {
          for (u32 l = 0; l < qx; l++) {

            if (l == qx - 1 || k == qy - 1) {
              u32 pixel = 0x0000FF00;
              set_pixel(quad_s * j + l, quad_s * i + k, ret, pixel);
            }
          }
        }
#endif
      }
    }
  }
  free(str.ptr);
  return ret;
}
void free_image(image *img) {
  for (u32 i = 0; i < img->length; i++) {
    free(img->parts[i].pixels.ptr);
    free(img->parts[i].blokcs.ptr);
  }
  free(img->parts);
  free(img->dicts);

  free(img);
}
u8 merge_dicts(dict8 *dst, dict8 *src, stream *pixels) {
  u8 matches = 0;
  u8 in[256];
  u8 out[256];
  u8 changed[256];
  for (u32 i = 0; i < 256; i++) {
    changed[i] = 0;
  }
  for (u32 i = 0; i < dst->size; i++) {
    for (u32 j = 0; j < src->size; j++) {

      if (dst->colors[i] == src->colors[j]) {
        in[matches] = j;
        out[matches] = i;
        changed[j] = 1;
        matches++;
      }
    }
  }
  if ((i32)dst->size + (i32)src->size - matches >= 255) {
    return 0;
  } else {
    for (u32 i = 0; i < (u32)src->size; i++) {
      if (changed[i] == 0) {
        dst->colors[dst->size] = src->colors[i];
        in[matches] = i;
        out[matches] = dst->size;
        changed[i] = 1;
        matches++;
        dst->size++;
      }
    }
  }
  for (u32 i = 0; i < pixels->size; i++) {
    if (changed[pixels->ptr[i]] == 1) {
      for (u32 j = 0; j < matches; j++) {
        if (pixels->ptr[i] == in[j]) {
          pixels->ptr[i] = out[j];
          break;
        }
      }
    }
  }

  return 1;
}
u32 average_color(rect area, bitmap *b) {
  u32 avg[4];
  memset(avg, 0, sizeof(u32) * 4);
  u8 esize = format_bpp(b->format);
  for (u32 i = area.y; i < area.y + area.h; i++) {
    for (u32 j = area.x; j < area.x + area.w; j++) {
      u32 pixel = get_pixel(j, i, b);
      for (u32 k = 0; k < esize; k++) {
        u8 *channel = (u8 *)&pixel;
        avg[k] += *(channel + k);
      }
    }
  }
  u32 res_avg = 0;
  for (u32 k = 0; k < esize; k++) {
    *(((u8 *)&res_avg) + k) = roundf(avg[k] / (float)(area.w * area.h));
  }
  return res_avg;
}
