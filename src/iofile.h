#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>

struct image;
typedef struct image image;

void write(stream *wr, char *path);
stream *read(char *path);
SDL_Surface *image_to_sdl(image *img);
image *sdl_to_image(SDL_Surface *surf);