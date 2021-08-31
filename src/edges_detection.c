#include "image.h"
#include "iofile.h"
#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
int main(int argc, char **argv) {
  SDL_Surface *s = IMG_Load(argv[1]);
  bitmap *b = sdl_to_bitmap(s);
  SDL_FreeSurface(s);

  bitmap *yuv_b = rgb_to_yuv(b);
  free_bitmap(b);
  rect reg;
  reg.x = 0;
  reg.y = 0;
  reg.w = yuv_b->x;
  reg.h = yuv_b->y;
  bitmap *edges = create_bitmap(yuv_b->x, yuv_b->y, yuv_b->x, RGB24);
  for (u32 i = 0; i < edges->y; i++) {
    for (u32 j = 0; j < edges->x; j++) {
      u32 color = edge_detection_yuv(yuv_b, j, i);
      color *= 100;
      set_pixel(j, i, edges, color);
    }
  }
  SDL_Surface *surf = bitmap_to_sdl(edges);
  SDL_SaveBMP(surf, "test.bmp");
  SDL_FreeSurface(surf);
  return 0;
}
