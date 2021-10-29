#include "../src/iofile.h"
#include "../src/types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>
#include <alloca.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

stream dct_quatization_matrix(u8 sizex, u8 sizey, u8 start, u8 end, u8 offset) {

	stream ret;
	ret.size = sizex * sizey;
	ret.ptr = malloc(ret.size);
	u8 start_end_diff = end - start;
	for (u32 i = 0; i < sizey; i++) {
		for (u32 j = 0; j < sizex; j++) {
			float distance = pow(((i * i + j * j) / (float)(sizex * sizex + sizey * sizey)), 3.0f);
			ret.ptr[i * sizex + j] = start + start_end_diff * distance;
		}
	}
	return ret;
}
void dct_quatization(u32 sizex, u32 sizey, u8 *dct, u8 *quants, u8 channels) {
	for (u32 c = 0; c < channels; c++) {
		for (u32 i = 0; i < sizey; i++) {
			for (u32 j = 0; j < sizex; j++) {
				dct[i * sizex + j + c * sizex * sizey] = (dct[i * sizex + j + c * sizex * sizey] / quants[i * sizex + j]) * quants[i * sizex + j];
			}
		}
	}
}
void xy_to_zigzag(void *matrix, u32 esize, u32 sizex, u32 sizey) {
	u8 *tmp = alloca(esize * sizex * sizey);
	u8 direction = 0;
	u32 offset = 0;
	u32 posx = 0;
	u32 posy = 0;
	while (offset < esize * sizex * sizey) {
		u32 index = (posx + posy * sizex) * esize;
		dstoffsetcopy(tmp, (u8 *)matrix + index, &offset, esize);
		if (direction) {
			if (posy == sizey - 1) {
				direction = 0;
				posx++;
			} else if (posx == 0) {
				direction = 0;
				if (posy == sizey - 1) {
					posx++;
				} else {
					posy++;
				}
			} else {
				posx--;
				posy++;
			}
		} else {
			if (posy == 0) {
				if (posx == sizex - 1) {
					posy++;
				} else {
					posx++;
				}
				direction = 1;
			} else if (posx == sizex - 1) {
				posy++;
				direction++;
			} else {
				posx++;
				posy--;
			}
		}
	}
	memcpy(matrix, tmp, esize * sizex * sizey);
}
void zigzag_to_xy(void *matrix, u32 esize, u32 sizex, u32 sizey) {
	u8 *tmp = alloca(esize * sizex * sizey);
	u8 direction = 0;
	u32 offset = 0;
	u32 posx = 0;
	u32 posy = 0;
	while (offset < esize * sizex * sizey) {
		u32 index = (posx + posy * sizex) * esize;
		srcoffsetcopy(tmp + index, matrix, &offset, esize);
		if (direction) {
			if (posy == sizey - 1) {
				direction = 0;
				posx++;
			} else if (posx == 0) {
				direction = 0;
				if (posy == sizey - 1) {
					posx++;
				} else {
					posy++;
				}
			} else {
				posx--;
				posy++;
			}
		} else {
			if (posy == 0) {
				if (posx == sizex - 1) {
					posy++;
				} else {
					posx++;
				}
				direction = 1;
			} else if (posx == sizex - 1) {
				posy++;
				direction++;
			} else {
				posx++;
				posy--;
			}
		}
	}
	memcpy(matrix, tmp, esize * sizex * sizey);
}
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
				out[area.w * area.h * c + area.w * y + x] = (fval + 1.0f) * (255.0f / 2.0f);
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
				dct_mat[area.w * area.h * c + area.w * y + x] = (in[area.w * area.h * c + area.w * y + x] / (255.0f / 2.0f) - 1.0f) * 4.0f;
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
	FILE *f = fopen("prequnat", "wb");
	fwrite(dct_matrix.ptr, b->x * b->y * 3, 1, f);
	fclose(f);
	bitmap *b2 = create_bitmap(b->x, b->y, b->x, RGB24);
	stream quant = dct_quatization_matrix(b->x, b->y, 1, 30, 0);
	dct_quatization(b->x, b->x, dct_matrix.ptr, quant.ptr, 3);
	f = fopen("quant", "wb");
	fwrite(dct_matrix.ptr, b->x * b->y * 3, 1, f);
	fclose(f);
	bitmap *b3 = create_bitmap(b->x, b->y, b->x, RGB24);
	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			u8 abc = quant.ptr[i * b->x + j];
			union piexl_data data;
			data.channels[0] = abc;
			data.channels[1] = abc;
			data.channels[2] = abc;
			set_pixel(j, i, b3, data.pixel);
		}
	}
	offset = 0;
	xy_to_zigzag(dct_matrix.ptr, 1, b->x, b->y);
	xy_to_zigzag(dct_matrix.ptr + b->x * b->y, 1, b->x, b->y);
	xy_to_zigzag(dct_matrix.ptr + b->x * b->y * 2, 1, b->x, b->y);

	f = fopen("zigzag", "wb");
	fwrite(dct_matrix.ptr, b->x * b->y * 3, 1, f);
	fclose(f);
	zigzag_to_xy(dct_matrix.ptr, 1, b->x, b->y);
	zigzag_to_xy(dct_matrix.ptr + b->x * b->y, 1, b->x, b->y);
	zigzag_to_xy(dct_matrix.ptr + b->x * b->y * 2, 1, b->x, b->y);

	idct(dct_matrix, &offset, area, b2);

	SDL_Surface *out = bitmap_to_sdl(b2);
	SDL_SaveBMP(out, "dct.bmp");
	out = bitmap_to_sdl(b3);
	SDL_SaveBMP(out, "dct_mat.bmp");

	return 0;
}