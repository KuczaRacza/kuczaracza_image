#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>

struct image;
typedef struct image image;

void write(stream *wr, char *path);
stream read(char *path);
SDL_Surface *bitmap_to_sdl(bitmap *img);
bitmap *sdl_to_bitmap(SDL_Surface *surf);
