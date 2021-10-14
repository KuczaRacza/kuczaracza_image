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
			union piexl_data px;
			for (u32 c = 0; c < 3; c++) {
				px.channels[c] = mat;
			}
			set_pixel(i, j, b, px.pixel);
		}
	}
	return b;
}
float **preapare_matix(bitmap *b, u32 dimX, u32 dimY) {

	float **m = calloc(dimX, sizeof(float *));
	float *p = calloc(dimX * dimY, sizeof(float));
	int i;
	for (i = 0; i < dimX; i++) {
		m[i] = &p[i * dimY];
	}
	if (b != NULL) {

		for (u32 i = 0; i < b->x; i++) {
			for (u32 j = 0; j < b->y; j++) {
				union piexl_data color;
				color.pixel = get_pixel(i, j, b);
				m[i][j] = color.rgba.r;
			}
		}
	}
	return m;
}
void write_mat(FILE *f, float **mat, u32 x, u32 y) {

	for (u32 i = 0; i < y; i++) {
		for (u32 j = 0; j < x; j++) {
			fprintf(f, "%f\t", mat[j][i]);
		}
		fprintf(f, "\n");
	}
}
int main(int argc, char **argv) {
	FILE * f = fopen("mat.txt", "w");
	SDL_Surface *img = IMG_Load(argv[1]);
	bitmap *b_r = sdl_to_bitmap(img);
	float **matrix = preapare_matix(b_r, b_r->x, b_r->y);
	write_mat(f, matrix, b_r->x, b_r->y);
	float **DCT_matrix = preapare_matix(NULL, b_r->x, b_r->y);
	dct(DCT_matrix, matrix, b_r->x, b_r->y);
	free(matrix);
	matrix = preapare_matix(NULL, b_r->x, b_r->y);
	idct(matrix, DCT_matrix, b_r->x, b_r->y);
	write_mat(f, matrix, b_r->x, b_r->y);
	bitmap *b = prepare_image(matrix, img->w, img->h);
	SDL_Surface *srf = bitmap_to_sdl(b);
	SDL_SaveBMP(srf, "aaaaa.bmp");
	return 0;
}