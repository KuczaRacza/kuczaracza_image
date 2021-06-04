#pragma once
#include "types.h"
enum compression : u8 {
  PALLETE = 0x1,
  QUANTIZATION = 0x2,
  QUAD_TREE = 0x4,
  BLOCKS = 0x8
};
typedef enum compression compression;
struct image {
  u32 size_x;
  u32 size_y;
  compression comp;
  bitmap pixels;

};
typedef struct image image;

image * encode(bitmap * raw);
stream serialize(image * img);
image *  deserialize(stream str);
