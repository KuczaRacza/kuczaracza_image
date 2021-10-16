#include "../src/iofile.h"
#include "../src/types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <math.h>

#include <stdio.h>
#include <string.h>

stream dct(bitmap *b) {
	stream str;
	str.size = b->x * b->y * sizeof(double);
	str.ptr = calloc(str.size, 1);
	double *dct_matrix = (double *)str.ptr;

	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			for (u32 y = 0; y < b->y; y++) {
				for (u32 x = 0; x < b->x; x++) {
					union piexl_data pixel;
					pixel.pixel = get_pixel(x, y, b);
					double normalized = (pixel.rgba.r / 127.5f) - 1.0f;
					dct_matrix[i * b->x + j] += normalized * cos((M_PI /(double) b->x) * (x + 0.5f) * j) * cos((M_PI / (double)b->y) * (y + 0.5f) * i) / (double)(b->x * b->y) * 4.0f;
				}
			}
		}
	}
	return str;
}
bitmap *idct(stream dct_matrix, u32 size_x, u32 size_y) {
	bitmap *b = create_bitmap(size_x, size_y, size_x, RGB24);
	stream idct_matrx;
	idct_matrx.ptr = calloc(sizeof(double), size_x * size_y);
	idct_matrx.size = sizeof(double) * size_x * size_y;
	double *pixel = (double *)idct_matrx.ptr;
	double *dct_mat = (double *)dct_matrix.ptr;
	for (u32 i = 1; i < size_y; i++) {
		for (u32 j = 1; j < size_x; j++) {
			for (u32 y = 0; y < size_y; y++) {
				for (u32 x = 0; x < size_x; x++) {
					double normalized = dct_mat[i * size_x + j];
					normalized = normalized * cos((M_PI / b->x) * (x + 0.5f) * j) * cos((M_PI / b->y) * (y + 0.5f) * i);
					pixel[y * size_x + x] += normalized;
				}
			}
		}
	}

	for (u32 i = 1; i < size_y; i++) {
		for (u32 y = 0; y < size_y; y++) {
			for (u32 x = 0; x < size_x; x++) {
				double normalized = dct_mat[i * size_x];
				normalized = normalized * 0.5f * cos((M_PI / b->y) * (y + 0.5f) * i);
				pixel[y * size_x + x] += normalized;
			}
		}
	}
	for (u32 i = 1; i < size_x; i++) {
		for (u32 y = 0; y < size_y; y++) {
			for (u32 x = 0; x < size_x; x++) {
				double normalized = dct_mat[i];
				normalized = normalized * 0.5f *  cos((M_PI / b->x) * (x + 0.5f) * i);
				pixel[y * size_x + x] += normalized;
			}
		}
	}

	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			double normalized = dct_mat[0];
			normalized = normalized;
			pixel[j * size_y + i] += 0.25f * normalized;
		}
	}
	for (u32 y = 0; y < size_y; y++) {
		for (u32 x = 0; x < size_x; x++) {
			union piexl_data px;
			double normalized = (pixel[y * size_x + x] + 1.0f) * 127.5f;
			if(normalized >255){
				normalized = 255;
			}
			else if(normalized <0){
				normalized =0;
			}
			px.rgba.r = normalized;
			px.rgba.g = px.rgba.r;
			px.rgba.b = px.rgba.r;
			px.rgba.a = 255;
			set_pixel(x, y, b, px.pixel);
		}
	}
	return b;
}
int main(int argc, char **argv) {
	SDL_Surface *surf = IMG_Load(argv[1]);
	bitmap *b = sdl_to_bitmap(surf);
	stream dct_s = dct(b);

	bitmap *b2 = idct(dct_s, b->x, b->y);
	SDL_Surface *out = bitmap_to_sdl(b2);
	SDL_SaveBMP(out, "dct.bmp");
	return 0;
}