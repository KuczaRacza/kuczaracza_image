#include "../src/iofile.h"
#include "../src/types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <alloca.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void dct(bitmap *b, rect area, stream str, u32 *offset) {
	u32 esize = format_bpp(b->format);
	float *dct_matrix = alloca(sizeof(float) * area.w * area.h * esize);
	memset(dct_matrix, 0, sizeof(float) * area.w * area.h * esize);
	for (u32 i = 0; i < area.h; i++) {
		for (u32 j = 0; j < area.w; j++) {
			for (u32 y = 0; y < area.h; y++) {
				for (u32 x = 0; x < area.w; x++) {
					union piexl_data pixel;
					pixel.pixel = get_pixel(x + area.x, y + area.y, b);
					for (u32 c = 0; c < esize; c++) {
						float normalized = (pixel.channels[c] / 127.5f) - 1.0f;
						dct_matrix[i * area.w + j + area.w * area.h * c] += normalized * cosf((M_PI / (float)area.w) * (x + 0.5f) * j) * cosf((M_PI / (float)area.h) * (y + 0.5f) * i) / (float)(area.w * area.h);
					}
				}
			}
		}
	}
	u16 *out = (u16 *)(str.ptr + *offset);
	for (u32 c = 0; c < esize; c++) {
		for (u32 y = 0; y < area.h; y++) {
			for (u32 x = 0; x < area.w; x++) {
				float fval = dct_matrix[area.w * area.h * c + area.w * y + x];
				if (fval < -1.0f) {
					fval = -1.0f;
				} else if (fval > 1.0f) {
					fval = 1.0f;
				}
				out[area.w * area.h * c + area.w * y + x] = (fval + 1.0f) * (511.0f / 2.0f);
			}
		}
		*offset += area.w * area.h * sizeof(u16) * esize;
	}
}
void idct(stream dct_matrix, u32 *offset, rect area, bitmap *b) {
	u32 esize = format_bpp(b->format);
	stream idct_matrix;
	idct_matrix.ptr = alloca(sizeof(float) * area.w * area.h * esize);
	idct_matrix.size = sizeof(float) * area.w * area.h * esize;
	memset(idct_matrix.ptr, 0, idct_matrix.size);
	float *dct_mat = alloca(esize * area.w * area.h * sizeof(float));
	u16 *in = (u16 *)(dct_matrix.ptr + *offset);
	for (u32 c = 0; c < esize; c++) {
		for (u32 y = 0; y < area.h; y++) {
			for (u32 x = 0; x < area.w; x++) {
				dct_mat[area.w * area.h * c + area.w * y + x] = (in[area.w * area.h * c + area.w * y + x] / (511.0f / 2.0f) - 1.0f) * 4.0f;
			}
		}
	}
	float *pixel = (float *)idct_matrix.ptr;
	for (u32 i = 1; i < area.h; i++) {
		for (u32 j = 1; j < area.w; j++) {
			for (u32 y = 0; y < area.h; y++) {
				for (u32 x = 0; x < area.w; x++) {
					for (u32 c = 0; c < esize; c++) {
						float normalized = dct_mat[i * area.w + j + area.w * area.h * c];
						normalized = normalized * cosf((M_PI / area.w) * (x + 0.5f) * j) * cosf((M_PI / area.h) * (y + 0.5f) * i);
						pixel[y * area.w + x + area.w * area.h * c] += normalized;
					}
				}
			}
		}
	}

	for (u32 i = 1; i < area.h; i++) {
		for (u32 y = 0; y < area.h; y++) {
			for (u32 x = 0; x < area.w; x++) {
				for (u32 c = 0; c < esize; c++) {
					float normalized = dct_mat[i * area.w + area.w * area.h * c];
					normalized = normalized * 0.5f * cosf((M_PI / area.h) * (y + 0.5f) * i);
					pixel[y * area.w + x + area.w * area.h * c] += normalized;
				}
			}
		}
	}
	for (u32 i = 1; i < area.w; i++) {
		for (u32 y = 0; y < area.h; y++) {
			for (u32 x = 0; x < area.w; x++) {
				for (u32 c = 0; c < esize; c++) {
					float normalized = dct_mat[i + area.w * area.h * c];
					normalized = normalized * 0.5f * cosf((M_PI / area.w) * (x + 0.5f) * i);
					pixel[y * area.w + x + area.w * area.h * c] += normalized;
				}
			}
		}
	}

	for (u32 i = 0; i < area.h; i++) {
		for (u32 j = 0; j < area.w; j++) {
			for (u32 c = 0; c < esize; c++) {
				float normalized = dct_mat[area.w * area.h * c];
				normalized = normalized;
				pixel[i * area.w + j + area.w * area.h * c] += 0.25f * normalized;
			}
		}
	}
	for (u32 y = 0; y < area.h; y++) {
		for (u32 x = 0; x < area.w; x++) {
			union piexl_data px;
			for (u32 c = 0; c < esize; c++) {
				float normalized = (pixel[y * area.w + x + area.w * area.h * c] + 1.0f) * 127.5f;
				if (normalized > 255) {
					normalized = 255;
				} else if (normalized < 0) {
					normalized = 0;
				}
				px.channels[c] = normalized;
			}
			set_pixel(x, y, b, px.pixel);
		}
	}
	*offset += sizeof(u8) * area.w * area.h * esize;
}
int main(int argc, char **argv) {
	SDL_Surface *surf = IMG_Load(argv[1]);
	bitmap *b = sdl_to_bitmap(surf);
	stream dct_matrix;
	dct_matrix.ptr = calloc(b->x * b->y, sizeof(float) * format_bpp(b->format));
	u32 offset = 0;
	rect area;
	area.x = 0;
	area.y = 0;
	area.w = b->x;
	area.h = b->y;
	dct(b, area, dct_matrix, &offset);

	bitmap *b2 = create_bitmap(b->x, b->y, b->x, RGB24);
	offset = 0;
	idct(dct_matrix, &offset, area, b2);
	SDL_Surface *out = bitmap_to_sdl(b2);
	SDL_SaveBMP(out, "dct.bmp");
	return 0;
}