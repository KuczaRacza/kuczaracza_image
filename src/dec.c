#include "image.h"
#include "iofile.h"
#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "wrong arguments");
    return 1;
  }
  stream raw = read(argv[1]);
  image *img = deserialize(raw);
  bitmap *b = decode(img);
  SDL_Surface *surf = bitmap_to_sdl(b);
  SDL_SaveBMP(surf, argv[2]);
  SDL_FreeSurface(surf);
  free_image(img);
  free(raw.ptr);
  free_bitmap(b);
  return 0;
}
