#include "image.h"
#include "iofile.h"
#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
   char *imoprt_path = NULL;
   char *export_path = NULL;
  for (uint16_t i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      printf(
          "decodes image .kczi into  bmp image\n dec  -i <input> "
          "-o <output>\n options:\n -h    help\n -o    output filename\n ");
      return 0;
    } else if (strcmp(argv[i], "-o") == 0) {
      export_path = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-i") == 0) {
      imoprt_path = argv[i + 1];
      i++;
    } else if (argv[i][0] == '-') {
      printf("incorrect switch %s", argv[i]);
      return 0;
    }
  }
  if (export_path == NULL) {
    printf("usage: dec -i <input> -o <output>\n -h help\n");
    return 0;
  }
  if (imoprt_path == NULL) {
    printf("usage: dec -i <input> -o <output>\n -h help\n");
    return 0;
  }
  stream raw = read(imoprt_path);
  image *img = deserialize(raw);
  bitmap *b = decode(img);
  SDL_Surface *surf = bitmap_to_sdl(b);
  SDL_SaveBMP(surf, export_path);
  SDL_FreeSurface(surf);
  free_image(img);
  free(raw.ptr);
  free_bitmap(b);
  return 0;
}
