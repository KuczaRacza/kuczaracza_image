#include "../src/iofile.h"
#include "../src/types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
void idct(float **Matrix, float **DCTMatrix, int N, int M) {
	int i, j, u, v;

	for (u = 0; u < N; ++u) {
		for (v = 0; v < M; ++v) {
			Matrix[u][v] = 1 / 4. * DCTMatrix[0][0];
			for (i = 1; i < N; i++) {
				Matrix[u][v] += 1 / 2. * DCTMatrix[i][0];
			}
			for (j = 1; j < M; j++) {
				Matrix[u][v] += 1 / 2. * DCTMatrix[0][j];
			}

			for (i = 1; i < N; i++) {
				for (j = 1; j < M; j++) {
					Matrix[u][v] += DCTMatrix[i][j] * cos(M_PI / ((float)N) * (u + 1. / 2.) * i) * cos(M_PI / ((float)M) * (v + 1. / 2.) * j);
				}
			}
			Matrix[u][v] *= 2. / ((float)N) * 2. / ((float)M);
		}
	}
}
void dct(float **DCTMatrix, float **Matrix, int N, int M) {

	int i, j, u, v;
	for (u = 0; u < N; ++u) {
		for (v = 0; v < M; ++v) {
			DCTMatrix[u][v] = 0;
			for (i = 0; i < N; i++) {
				for (j = 0; j < M; j++) {
					DCTMatrix[u][v] += Matrix[i][j] * cos(M_PI / ((float)N) * (i + 1. / 2.) * u) * cos(M_PI / ((float)M) * (j + 1. / 2.) * v);
				}
			}
		}
	}
}
bitmap *prepare_image(float **matrix, u32 size_x, u32 size_y) {
	bitmap *b = create_bitmap(size_x, size_y, size_x * 3, RGB24);
	for (u32 i = 0; i < size_x; i++) {
		for (u32 j = 0; j < size_y; j++) {
			float mat = matrix[i][j];
			mat = mat * 255.0f;
			union piexl_data px;
			for (u32 c = 0; c < 3; c++) {
				px.channels[c] = mat;
			}
			set_pixel(i, j, b, px.pixel);
		}
	}
	return b;
}
float **preapare_matix(bitmap *b) {

	float **matrix = (float **)malloc(sizeof(float *) * b->x);
	for (u32 i = 0; i < b->x; i++) {
		matrix[i] = malloc(sizeof(float) * b->y);
		for (u32 j = 0; j < b->y; j++) {
			union piexl_data color_ptr;
			u32 col = 0;
			color_ptr.pixel = get_pixel(i, j, b);
			for (u32 c = 0; c < 3; c++) {
				col += color_ptr.channels[c];
			}
			matrix[i][j] = (float)col / 3.0f;
		}
	}
	return matrix;
}
int main(int argc, char **argv) {
	SDL_Surface *img = IMG_Load(argv[1]);
	bitmap *b_r = sdl_to_bitmap(img);
	float **matrix = preapare_matix(b_r);
	float **DCT_matrix = malloc(b_r->y * sizeof(float *));
	for (u32 i = 0; i < b_r->y; i++) {
		DCT_matrix[i] = malloc(sizeof(float) * b_r->x);
	}
	dct(DCT_matrix, matrix, b_r->x, b_r->y);
	for (u32 i = 0; i < b_r->y; i++) {
		free(matrix[i]);
	}
	free(matrix);
	bitmap *b = prepare_image(matrix, img->w, img->h);
	SDL_Surface *srf = bitmap_to_sdl(b);
	SDL_SaveBMP(srf, "aaaaa.bmp");
	return 0;
}