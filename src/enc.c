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
  SDL_Surface *surf = IMG_Load(argv[1]);
  bitmap *b = sdl_to_bitmap(surf);
  image *img = encode(b);
  stream str = seralize(img);
  write(&str, argv[2]);
  free(str.ptr);
  SDL_FreeSurface(surf);
  free(b->ptr);
  free(b);
  return 0;
}
