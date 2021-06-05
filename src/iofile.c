#include "iofile.h"
#include "image.h"
#include "types.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void write(stream *wr, char *path) {
  FILE *f = fopen(path, "wb");
  fwrite(&wr->size, 1, sizeof(u64), f);
  fwrite(wr->ptr, wr->size, sizeof(u8), f);
  fclose(f);
}
stream read(char *path) {
  FILE *f = fopen(path, "rb");
  stream str;
  fread(&str.size, 1, sizeof(u64), f);
  if (str.size > 512 * 1024 * 1024) {
    fprintf(stderr, "file too big; may be corrupted\n");
    return str;
  }
  str.ptr = (u8 *)malloc(str.size);
  fread(str.ptr, 1, str.size, f);
  fclose(f);
  return str;
}
SDL_Surface *bitmap_to_sdl(bitmap *img) {
  u32 bpp = format_bpp(img->format);
  i32 pixelformat;
  switch (bpp) {
  case 3:
    pixelformat = SDL_PIXELFORMAT_RGB24;
    break;
  case 4:
    pixelformat = SDL_PIXELFORMAT_RGBA32;
	break;
  default:
    fprintf(stderr, "format not supported");
    return NULL;
    break;
  }
  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
      img->ptr, img->x, img->y, bpp * 8, img->row * bpp, pixelformat);
  return surf;
}
bitmap *sdl_to_bitmap(SDL_Surface *surf) {
  pixel_format fmt;
  switch (surf->format->format) {
  case SDL_PIXELFORMAT_RGBA32:
  case SDL_PIXELFORMAT_RGBA8888:
    fmt = RGBA32;
    break;
  case SDL_PIXELFORMAT_RGB888:
  case SDL_PIXELFORMAT_RGB24:
    fmt = RGB24;
    break;
  default:
    fprintf(stderr, "format not supported");
    return NULL;
    break;
  }

  bitmap *img = create_bitmap(surf->w, surf->h,surf->pitch/format_bpp(fmt), fmt);
  memcpy(img->ptr, surf->pixels, img->y * img->row * format_bpp(img->format));
  return img;
  // work in progress
}
