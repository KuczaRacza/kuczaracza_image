#include "iofile.h"
#include "image.h"
#include "types.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>
#include <stdlib.h>
void write(stream *wr, char *path) {
  FILE *f = fopen(path, "wb");
  fwrite(&wr->size, 1, sizeof(u64), f);
  fwrite(wr->ptr, wr->size, sizeof(u8), f);
  fclose(f);
}
stream *read(char *path) {
  FILE *f = fopen(path, "rb");
  stream *str = (stream *)malloc(sizeof(stream));
  fread(&str->size, 1, sizeof(u64), f);
  if (str->size > 512 * 1024 * 1024) {
    return NULL;
  }
  str->ptr = (u8 *)malloc(str->size);
  fread(str->ptr, 1, str->size, f);
  fclose(f);
  return str;
}
SDL_Surface *image_to_sdl(image *img) {
  u32 bpp = format_bpp(img->pixels.format);
  i32 pixelformat;
  switch (bpp) {
  case 3:
    pixelformat = SDL_PIXELFORMAT_RGB24;
    break;
  case 4:
    pixelformat = SDL_PIXELFORMAT_RGBA8888;
  default:
    fprintf(stderr, "format not supported");
    return NULL;
    break;
  }
  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
      img->pixels.ptr, img->size_x, img->size_y, bpp * 8, img->pixels.x * bpp,
      pixelformat);
  return surf;
}
image *sdl_to_image(SDL_Surface *surf) {
  pixel_format fmt;
  switch (surf->format->format) {
  case SDL_PIXELFORMAT_RGBA8888:
    fmt = RGBA32;
    break;
  case SDL_PIXELFORMAT_RGB24:
    fmt = RGB24;
    break;
  default:
    fprintf(stderr, "format not supported");
    return NULL;
    break;
  }
  //work in progress
}