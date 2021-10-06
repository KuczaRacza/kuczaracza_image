#include "image.h"
#include "consts.h"
#include "types.h"
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_pixels.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

#define CLIP(X) ((X) > 255 ? 255 : (X) < 0 ? 0 \
																					 : X)

// YUV -> RGB
#define C(Y) ((Y)-16)
#define D(U) ((U)-128)
#define E(V) ((V)-128)

#define YUV2R(Y, U, V) CLIP((298 * C(Y) + 409 * E(V) + 128) >> 8)
#define YUV2G(Y, U, V) CLIP((298 * C(Y) - 100 * D(U) - 208 * E(V) + 128) >> 8)
#define YUV2B(Y, U, V) CLIP((298 * C(Y) + 516 * D(U) + 128) >> 8)
// RGB -> YUV
#define RGB2Y(R, G, B) CLIP(((66 * (R) + 129 * (G) + 25 * (B) + 128) >> 8) + 16)
#define RGB2U(R, G, B) CLIP(((-38 * (R)-74 * (G) + 112 * (B) + 128) >> 8) + 128)
#define RGB2V(R, G, B) CLIP(((112 * (R)-94 * (G)-18 * (B) + 128) >> 8) + 128)

image *encode(bitmap *raw, u32 max_block_size, u32 color_reduction,
							u32 block_color_sensitivity, u32 complexity) {
	// allocate struct
	image *img = (image *)malloc(sizeof(image));
	// set basic info
	img->format = raw->format;
	img->size_x = raw->x;
	img->size_y = raw->y;
	img->blok_size = max_block_size;
	img->color_quant = color_reduction;
	img->color_sensitivity = block_color_sensitivity;
	// copy bitmap to avoid input overwrite
	bitmap *copy = copy_bitmap(raw, 0, 0, raw->x, raw->y);
	if (copy->x > SMALL_IMAGE_LIMIT || copy->y > SMALL_IMAGE_LIMIT && copy->format != RGBA32) {
		copy->format = YUV444;
		bitmap *yuv_bitmap = rgb_to_yuv(copy);
		free_bitmap(copy);
		copy = yuv_bitmap;
		img->format = YUV444;
		edeges_map(img, copy);
	} else {
		if (copy->format == RGBA32) {
			img->format = DICTRGBA;
		} else {
			img->format = DICTRGB;
		}
	}

	// reduce clors
	if (color_reduction > 1) {
		linear_quantization(copy, color_reduction, 0);
	}

	// compress
	rectangle_tree(img, copy, max_block_size, block_color_sensitivity, complexity);
	free_bitmap(copy);

	return img;
}

bitmap *decode(image *img) {
	// create output bitmap
	u8 return_format = RGB24;
	if (img->format == RGBA32 || img->format == DICTRGBA) {
		return_format = RGBA32;
	}

	bitmap *map = create_bitmap(img->size_x, img->size_y, img->size_x, return_format);

	u32 esize = format_bpp(img->format);

	for (u32 i = 0; i < img->length; i++) {
		// translate from palette to real colors
		stream str;
		if (img->format == DICTRGB || img->format == DICTRGBA) {
			str = depalette(img->parts[i].pixels,
											&img->dicts[img->parts[i].dict_index], esize);
		} else {
			str.ptr = malloc(img->parts[i].pixels.size);
			str.size = img->parts[i].pixels.size;
			memcpy(str.ptr, img->parts[i].pixels.ptr, str.size);
		}
		// area of current part
		rect part_rect;
		part_rect.x = img->parts[i].x;
		part_rect.y = img->parts[i].y;
		part_rect.w = img->parts[i].w;
		part_rect.h = img->parts[i].h;
		// detrmining quad blocks size
		u32 quad = (1 - img->parts[i].depth / (float)img->max_depth) * img->blok_size;
		quad = (quad < 8) ? 8 : quad;
		quad /= 2;
		quad *= 2;
		// interpolating quads
		bitmap *btmp = recreate_quads(str, quad, part_rect, img->format, img->parts[i].blokcs);
		// copying pixels
		for (u32 j = 0; j < img->parts[i].h; j++) {
			for (u32 k = 0; k < img->parts[i].w; k++) {
				u32 color = get_pixel(k, j, btmp);
#if SHOW_BLOCKS
				if (j > img->parts[i].h - 3 || k > img->parts[i].w - 3) {
					color = 0x00FFFF00;
				}
#endif
				set_pixel(k + img->parts[i].x, j + img->parts[i].y, map, color);
			}
		}
		free_bitmap(btmp);
	}

	if (img->format == YUV444) {
		bitmap *rgb_map = yuv_to_rgb(map);
		free_bitmap(map);
		map = rgb_map;
	}

	return map;
}

stream seralize(image *img) {
	stream str;
	u32 esize = format_bpp(img->format);
	str.size = sizeof(u16) +	 // size_x
						 sizeof(u16) +	 // size_y
						 sizeof(u8) +		 // format
						 sizeof(u8) +		 // max depth
						 sizeof(u16) +	 // block size
						 sizeof(u16) +	 // quant
						 sizeof(u16) +	 // block sensitivity
						 sizeof(u32) +	 // length
						 sizeof(u32) +	 // dict length
														 // foreach part of image
						 (sizeof(u16) +	 // x part
							sizeof(u16) +	 // y part
							sizeof(u16) +	 // w part
							sizeof(u16) +	 // h part
							sizeof(u8) +	 // depth
							sizeof(u32) +	 // stream size
							sizeof(u32) +	 // block size
							sizeof(u16)) * // dict index
								 img->length;
	// calulating sie of pallets and pixels
	for (u32 i = 0; i < img->length; i++) {
		str.size += img->parts[i].pixels.size;
		str.size += img->parts[i].blokcs.size;
	}

	str.size += img->dicts_length * sizeof(u8);
	for (u32 i = 0; i < img->dicts_length; i++) {
		str.size += img->dicts[i].size * esize;
	}

	str.ptr = malloc(str.size);
	u32 offset = 0;
	dstoffsetcopy(str.ptr, &img->size_x, &offset, sizeof(u16));
	dstoffsetcopy(str.ptr, &img->size_y, &offset, sizeof(u16));
	dstoffsetcopy(str.ptr, &img->format, &offset, sizeof(u8));
	dstoffsetcopy(str.ptr, &img->max_depth, &offset, sizeof(u8));
	dstoffsetcopy(str.ptr, &img->blok_size, &offset, sizeof(u16));
	dstoffsetcopy(str.ptr, &img->color_quant, &offset, sizeof(u16));
	dstoffsetcopy(str.ptr, &img->color_sensitivity, &offset, sizeof(u16));
	dstoffsetcopy(str.ptr, &img->length, &offset, sizeof(u32));
	dstoffsetcopy(str.ptr, &img->dicts_length, &offset, sizeof(u32));
	// copying each part
	for (u32 i = 0; i < img->length; i++) {
		dstoffsetcopy(str.ptr, &img->parts[i].x, &offset, sizeof(u16));
		dstoffsetcopy(str.ptr, &img->parts[i].y, &offset, sizeof(u16));
		dstoffsetcopy(str.ptr, &img->parts[i].w, &offset, sizeof(u16));
		dstoffsetcopy(str.ptr, &img->parts[i].h, &offset, sizeof(u16));
		dstoffsetcopy(str.ptr, &img->parts[i].depth, &offset, sizeof(u8));
		dstoffsetcopy(str.ptr, &img->parts[i].dict_index, &offset, sizeof(u16));
		dstoffsetcopy(str.ptr, &img->parts[i].pixels.size, &offset, sizeof(u32));
		dstoffsetcopy(str.ptr, img->parts[i].pixels.ptr, &offset, img->parts[i].pixels.size);
		dstoffsetcopy(str.ptr, &img->parts[i].blokcs.size, &offset, sizeof(u32));
		dstoffsetcopy(str.ptr, img->parts[i].blokcs.ptr, &offset, img->parts[i].blokcs.size);
	}

	for (u32 i = 0; i < img->dicts_length; i++) {
		dstoffsetcopy(str.ptr, &img->dicts[i].size, &offset, sizeof(u8));
		for (u32 j = 0; j < img->dicts[i].size; j++) {
			dstoffsetcopy(str.ptr, &img->dicts[i].colors[j], &offset, esize);
		}
	}
	stream compressed_str;
	compressed_str.size = str.size;
	compressed_str.ptr = malloc(compressed_str.size);
	compressed_str.size = ZSTD_compress(compressed_str.ptr, compressed_str.size, str.ptr, str.size, 22);
	free(str.ptr);
	compressed_str.ptr = realloc(compressed_str.ptr, compressed_str.size + sizeof(u64));
	memcpy(compressed_str.ptr + compressed_str.size, &str.size, sizeof(u64));
	compressed_str.size += sizeof(u64);
	return compressed_str;
}

image *deserialize(stream compressed_str) {
	stream str;
	memcpy(&str.size, compressed_str.ptr + compressed_str.size - sizeof(u64), sizeof(u64));
	str.ptr = malloc(str.size);
	str.size = ZSTD_decompress(str.ptr, str.size, compressed_str.ptr, compressed_str.size - sizeof(u64));
	str.ptr = realloc(str.ptr, str.size);
	image *img = (image *)malloc(sizeof(image));
	u32 offset = 0;
	// copying basic info about image
	srcoffsetcopy(&img->size_x, str.ptr, &offset, sizeof(u16));
	srcoffsetcopy(&img->size_y, str.ptr, &offset, sizeof(u16));
	srcoffsetcopy(&img->format, str.ptr, &offset, sizeof(u8));
	srcoffsetcopy(&img->max_depth, str.ptr, &offset, sizeof(u8));
	srcoffsetcopy(&img->blok_size, str.ptr, &offset, sizeof(u16));
	srcoffsetcopy(&img->color_quant, str.ptr, &offset, sizeof(u16));
	srcoffsetcopy(&img->color_sensitivity, str.ptr, &offset, sizeof(u16));
	srcoffsetcopy(&img->length, str.ptr, &offset, sizeof(u32));
	srcoffsetcopy(&img->dicts_length, str.ptr, &offset, sizeof(u32));
	// size of each pixel
	u32 esize = format_bpp(img->format);
	img->parts = malloc(sizeof(region) * img->length);
	for (u32 i = 0; i < img->length; i++) {
		srcoffsetcopy(&img->parts[i].x, str.ptr, &offset, sizeof(u16));
		srcoffsetcopy(&img->parts[i].y, str.ptr, &offset, sizeof(u16));
		srcoffsetcopy(&img->parts[i].w, str.ptr, &offset, sizeof(u16));
		srcoffsetcopy(&img->parts[i].h, str.ptr, &offset, sizeof(u16));
		srcoffsetcopy(&img->parts[i].depth, str.ptr, &offset, sizeof(u8));
		srcoffsetcopy(&img->parts[i].dict_index, str.ptr, &offset, sizeof(u16));
		// copying pixels
		u32 ssize = 0;
		srcoffsetcopy(&ssize, str.ptr, &offset, sizeof(u32));
		img->parts[i].pixels.size = ssize;
		img->parts[i].pixels.ptr = malloc(ssize);
		srcoffsetcopy(img->parts[i].pixels.ptr, str.ptr, &offset, ssize);
		u32 bsize = 0;
		srcoffsetcopy(&bsize, str.ptr, &offset, sizeof(u32));
		img->parts[i].blokcs.size = bsize;
		img->parts[i].blokcs.ptr = malloc(bsize);
		srcoffsetcopy(img->parts[i].blokcs.ptr, str.ptr, &offset, bsize);
	}
	img->dicts = calloc(sizeof(dict8), img->dicts_length);
	for (u32 i = 0; i < img->dicts_length; i++) {
		srcoffsetcopy(&img->dicts[i].size, str.ptr, &offset, sizeof(u8));
		for (u32 j = 0; j < img->dicts[i].size; j++) {
			srcoffsetcopy(&img->dicts[i].colors[j], str.ptr, &offset, esize);
		}
	}
#if INFOLOGS

	if (str.size > 2000) {
		str.size /= 1024;
		printf("Size without zstd compression %lu KiB\n", str.size);
	} else {
		printf("Size without zstd compression %lu B\n", str.size);
	}

#endif
	free(str.ptr);
	return img;
}

void linear_quantization(bitmap *b, u32 quant, u8 alpha) {
	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			u32 color = get_pixel(j, i, b);
			u8 *col = (u8 *)&color;
			float q = quant;
			// omits alpha if not present in image
			for (u8 k = 0; k < ((alpha & 1) ? 4 : 3); k++) {
				float cl = roundf((float)col[k] / q) * (float)q;
				col[k] = (cl > 255) ? 255 : cl;
			}
			set_pixel(j, i, b, color);
		}
	}
}

stream palette(stream str, dict8 *d, u32 esize) {
	stream ret;
	// resetting palette size
	d->size = 0;
	// calulating size of pixel after palleting
	ret.size = str.size / esize;
	ret.ptr = malloc(ret.size);
	for (u32 i = 0; i < ret.size; i++) {
		u32 pixel;
		// copying each pixel in loop
		memcpy(&pixel, (u8 *)str.ptr + (i * esize), esize);
		// tring to add to palette
		u32 dcol = add_color(d, pixel);
		if (dcol == 256) {
			// overflow
			ret.ptr[i] = 0;
		} else {
			// writning down new paletted  color
			ret.ptr[i] = (u8)dcol;
		}
	}
	return ret;
}

stream depalette(stream str, dict8 *d, u32 esize) {
	stream ret;
	// calulating new size of deplleted pixels
	ret.size = str.size * esize;
	ret.ptr = malloc(ret.size);
	for (u32 i = 0; i < str.size; i++) {
		// lookup in palette;
		u32 dcol = str.ptr[i];
		u32 pixel = get_dict(dcol, d);
		// writing down
		memcpy((u8 *)ret.ptr + (esize * i), &pixel, esize);
	}
	return ret;
}

u16 add_color(dict8 *d, u32 color) {
	for (u16 i = 0; i < d->size; i++) {
		// cheking if color is already in palette
		if (d->colors[i] == color) {
			return i;
		}
	}
	// overflow
	if (d->size == 255) {
		return 256;
	}
	// addding color and returning new entry
	d->colors[d->size] = color;
	d->size++;
	return d->size - 1;
}
u32 get_dict(u8 index, dict8 *d) { return d->colors[index]; }

void rectangle_tree(image *img, bitmap *raw, u32 max_block_size,
										u32 block_color_sensitivity, u32 complexity) {
	// vector of rects
	vector vec;
	vec.size = 0;
	vec.capacity = 40;
	vec.data = malloc(40);
	// dimensions of image
	rect start_area;
	start_area.x = 0;
	start_area.y = 0;
	start_area.w = raw->x;
	start_area.h = raw->y;
	// recursive function that write to vec rect of every part
	create_rect(&vec, raw, start_area, 0, img->format, img->edges_map,
							img->avg_edges);
	// size of element in vec
	u32 element_size = sizeof(rect) + sizeof(u8);
	// vector size
	u32 length = vec.size / element_size;
	img->length = length;
	img->dicts_length = length;
	// allocating space needed to write down parts
	img->parts = malloc(sizeof(region) * length);
	img->dicts = calloc(sizeof(dict8), length);

	u8 max_depth = 0;
	// determing max depth of rectangular tree
	for (u32 i = 0; i < length; i++) {
		// reading depth in each entry in vec
		u8 td = *(u8 *)(vec.data + element_size * i + sizeof(rect));
		if (td > max_depth) {
			max_depth = td;
		}
	}
	// to avoid dividing by 0
	if (max_depth == 0) {
		max_depth = 1;
	}
	img->max_depth = max_depth;
	u32 dict_len = 0;
	// encoding each part
	for (u32 i = 0; i < length; i++) {
		// reading rect of rectangles
		rect trt = *(rect *)(vec.data + element_size * i);
		u8 td = *(u8 *)(vec.data + element_size * i + sizeof(rect));
		// setting rects
		img->parts[i].x = trt.x;
		img->parts[i].y = trt.y;
		img->parts[i].w = trt.w;
		img->parts[i].h = trt.h;
		img->parts[i].depth = td;

		bitmap *btmp = copy_bitmap(raw, trt.x, trt.y, trt.w, trt.h);
		stream stmp;
		// size of each quad in cuttnig qads
		u32 quad = (1 - td / (float)max_depth) * max_block_size;
		quad = (quad < 8) ? 8 : quad;
		quad /= 2;
		quad *= 2;
		u32 threshold = block_color_sensitivity + (1.0f - td / (float)max_depth) * block_color_sensitivity;
		// alocates array where algorithms foreach blocks are written
		u32 blsize = ((trt.w / quad == 0) ? 1 : trt.w / quad) * ((trt.h / quad == 0) ? 1 : trt.h / quad);
		if (blsize % 4 != 0) {
			blsize /= 4;
			blsize++;
		} else {
			blsize /= 4;
		}
		img->parts[i].blokcs.size = blsize;
		img->parts[i].blokcs.ptr = calloc(1, blsize);
		// pixels with cutted unnesseary quads
		stmp = cut_quads(btmp, quad, threshold, img->parts[i].blokcs, img, trt);
		// palletting pixels and witing to image struct
		if (img->format == DICTRGB || img->format == DICTRGBA) {
			img->parts[i].pixels =
					palette(stmp, &img->dicts[dict_len], format_bpp(btmp->format));
			if (dict_len > 0 && complexity > 0) {
				u8 merged = 0;
				for (i32 j = dict_len; j > 0; j--) {
					if (img->dicts[dict_len - j].size + img->dicts[dict_len].size < 300) {
						if (merge_dicts(&img->dicts[dict_len - j], &img->dicts[dict_len],
														&img->parts[i].pixels) == 1) {
							img->parts[i].dict_index = dict_len - j;
							merged = 1;
							break;
						}
					}
				}
				if (merged == 0) {
					img->parts[i].dict_index = dict_len;
					dict_len++;
				}

			} else {
				img->parts[i].dict_index = dict_len;
				dict_len++;
			}
			free(stmp.ptr);
		} else {
			img->parts[i].pixels = stmp;
		}
		free_bitmap(btmp);
	}
	if (img->format == DICTRGB || img->format == DICTRGBA) {
		img->dicts_length = dict_len;
		img->dicts = realloc(img->dicts, sizeof(dict8) * dict_len);
	} else {
		img->dicts_length = 0;
		free(img->dicts);
		img->dicts = NULL;
	}
	free(vec.data);
}

u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h) {
	u32 colors[256];
	u32 number = 0;
	for (u32 i = y; i < y + h; i++) {
		for (u32 j = x; j < x + w; j++) {
			u32 pixel = get_pixel(j, i, b);
			u8 unique = 1;
			for (u32 k = 0; k < number; k++) {
				// cheking for repeating colors
				if (colors[k] == pixel) {
					unique = 0;
					break;
				}
			}
			if (unique == 1) {
				colors[number] = pixel;
				// in case of reaching limit
				if (number == 255) {
					return 256;
				} else {
					colors[number] = pixel;
					number++;
				}
			}
		}
	}
	return number;
}

u64 edge_detection_yuv(bitmap *b, u32 x, u32 y) {
	const u32 distance = 2;
	u32 this_pixel = get_pixel(x, y, b);
	u8 pixel_y = *(u8 *)&this_pixel;
	u32 avrg = 0;
	u32 background_pixels[4];
	u8 avalible[4];
	memset(avalible, 0x0, sizeof(u8) * 4);
	// left upper
	if (x >= distance && y >= distance) {
		avalible[0] = 1;
		background_pixels[0] = get_pixel(x - distance, y - distance, b);
	}
	// right upper
	if (x + distance < b->x && y >= distance) {
		background_pixels[1] = get_pixel(x + distance, y - distance, b);
		avalible[1] = 1;
	}
	// left down
	if (x >= distance && y + distance < b->y) {
		background_pixels[2] = get_pixel(x - distance, y + distance, b);
		avalible[2] = 1;
	}
	// right down
	if (x + distance < b->x && y + distance < b->y) {
		background_pixels[3] = get_pixel(x + distance, y + distance, b);
		avalible[3] = 1;
	}
	u32 differences = 0;
	u32 avalible_n = 0;
	for (u32 k = 0; k < 4; k++) {
		if (avalible[k]) {
			u8 y_background = *(u8 *)&background_pixels[k];
			differences += abs(y_background - pixel_y);
			avalible_n++;
		}
	}
	differences = differences / avalible_n;
	return differences;
}
u32 count_colors(bitmap *b) { return count_colors_rect(b, 0, 0, b->x, b->y); }
void create_rect(vector *rects, bitmap *raw, rect area, u8 depth, u8 format,
								 stream edges, u32 avg_edges) {
	// counting colors
	u8 divide = 0;
	if (format == DICTRGBA || format == DICTRGB) {
		if (area.w > 16 || area.h > 16) {
			u32 cols = count_colors_rect(raw, area.x, area.y, area.w, area.h);
			if (cols > 255) {
				divide = 1;
			}
		}
	} else if (area.w > 64 || area.h > 64) {
		u64 edges_sum = 0;

		u32 lower_limit = 70 + depth * 12 + depth * depth * 4 +
											depth * depth * depth * depth * depth * 2;

		for (u32 i = area.y; i < area.y + area.h; i++) {
			for (u32 j = area.x; j < area.x + area.w; j++) {
				u32 local_edge = edges.ptr[i * raw->x + j];

				if (local_edge > avg_edges) {

					local_edge -= avg_edges;
				} else {
					local_edge = 0;
				}
				// TODO change to int
				edges_sum += local_edge * local_edge;
			}
		}
		edges_sum *= 950;
		edges_sum /= (area.h * area.w);
		if (edges_sum > lower_limit) {
			divide = 1;
		}
	}
	// if limit is reached
	if (divide) {
		// cereating two rects
		rect arect;
		rect brect;
		// if depth is even splits horizontal if odd splits vertical
		if (depth % 2) {

			arect.x = area.x;
			arect.y = area.y;
			arect.w = area.w / 2;
			arect.h = area.h;
			brect.x = area.x + area.w / 2;
			brect.y = area.y;
			brect.w = area.w - area.w / 2;
			brect.h = area.h;

		} else {
			arect.x = area.x;
			arect.y = area.y;
			arect.w = area.w;
			arect.h = area.h / 2;
			brect.x = area.x;
			brect.y = area.y + area.h / 2;
			brect.w = area.w;
			brect.h = area.h - area.h / 2;
		}
		// for splitted area checks if another splits are nessesary
		create_rect(rects, raw, arect, depth + 1, format, edges, avg_edges);
		create_rect(rects, raw, brect, depth + 1, format, edges, avg_edges);
	} else {
		// if color limit is not exceeded writes down area to vector
		push_vector(rects, &area, sizeof(area));
		push_vector(rects, &depth, sizeof(u8));
	}
}
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold, stream blocks, image *img,
								 rect area) {
	u64 avg_edges = 0;
	if (b->format == YUV444) {
		for (u32 i = area.y; i < area.y + area.h; i++) {
			for (u32 j = area.x; j < area.x + area.w; j++) {

				avg_edges += img->edges_map.ptr[i * img->size_x + j] * 10;
			}
		}
		avg_edges /= area.w * area.h;
	}
	// pixels are translated form bitamp into array where some blocks are skipped
	stream ret;
	u32 esize = format_bpp(b->format);
	ret.size = b->x * b->y * esize;
	ret.ptr = malloc(ret.size);
	u32 offset = 0;
	// for each block
	u32 x_blocks_size = b->x / quad_s;
	u32 y_blocks_size = b->y / quad_s;
	if (x_blocks_size == 0) {
		x_blocks_size = 1;
		quad_s = b->x;
	}
	if (y_blocks_size == 0) {
		y_blocks_size = 1;
		quad_s = b->y;
	}

	for (u32 i = 0; i < y_blocks_size; i++) {
		for (u32 j = 0; j < x_blocks_size; j++) {
			// corners
			u32 qx = quad_s;
			u32 qy = quad_s;

			if (i + 1 == y_blocks_size) {
				qy = b->y - quad_s * i;
			}
			if (j + 1 == x_blocks_size) {
				qx = b->x - quad_s * j;
			}

			// algorithm  used in this block
			u8 algo;

			if (b->format == YUV444) {
				u64 difference_sum = 0;
				for (u32 k = 0; k < qy; k++) {
					for (u32 l = 0; l < qx; l++) {
						u32 index = ((i * quad_s + k) + area.y) * img->size_x +
												(j * quad_s + l) + area.x;
						u32 pixel_edge = img->edges_map.ptr[index];
						if (pixel_edge > avg_edges) {
							pixel_edge -= avg_edges;
							difference_sum += powf(pixel_edge, 3.0f);
						}
					}
				}
				difference_sum = difference_sum * 800 / (b->x * b->y);

				if (difference_sum <
								threshold + threshold * avg_edges * avg_edges / 40 &&
						avg_edges < 60) {
					algo = 0;
				} else {
					if (i > 0 && j > 0 &&
							difference_sum <
									threshold + threshold * avg_edges * avg_edges / 40) {
						u16 pos = i * x_blocks_size + j - 1;
						u32 bitshift = (pos % 4 * 2);
						u8 left_block = (blocks.ptr[pos / 4] & (3 << bitshift)) >> bitshift;
						pos = (i - 1) * x_blocks_size + j;
						bitshift = (pos % 4 * 2);
						u8 up_block = (blocks.ptr[pos / 4] & (3 << bitshift)) >> bitshift;
						if (up_block == 1 && left_block == 1) {

							algo = 2;
#if NOPREDICTION
							algo = 1;
#endif
						} else {
							algo = 1;
						}
					} else {
						algo = 1;
					}
				}

			} else {
				u32 min_channel[4];
				u32 max_channel[4];
				memset(min_channel, 0xFFFFFFFF, sizeof(u32) * 4);
				memset(max_channel, 0x0, sizeof(u32) * 4);
				for (u32 k = 0; k < qy; k++) {
					for (u32 l = 0; l < qx; l++) {
						u32 pixel = get_pixel(j * quad_s + l, i * quad_s + k, b);

						for (u32 m = 0; m < esize; m++) {
							u8 *value = (u8 *)(&pixel) + m;
							if (*value > max_channel[m]) {
								max_channel[m] = *value;
							}
							if (*value < min_channel[m]) {
								min_channel[m] = *value;
							}
						}
					}
				}
				u32 biggest_difference = 0;
				for (u32 m = 0; m < esize; m++) {
					u32 this_difference = max_channel[m] - min_channel[m];
					if (this_difference > biggest_difference) {
						biggest_difference = this_difference;
					}
				}
				if (biggest_difference >= threshold) {
					algo = 1;
				} else {
					algo = 0;
				}
			}

			// caluclates biggest difference in color per channel

			// if  difference is bigger that means that block cannot be skipped
			if (algo == 1) {
				rect quad_rect;
				quad_rect.x = quad_s * j;
				quad_rect.y = quad_s * i;
				quad_rect.w = qx;
				quad_rect.h = qy;
#if NOSUBSAMPLING
				fullsampling(b, quad_rect, ret, &offset);
#else
				subsampling_yuv(b, quad_rect, ret, &offset);
#endif
			} else if (algo == 0) {
				u32 cr[4];
				memset(cr, 0, sizeof(u32) * 4);
				if (quad_s <= 8) {
					cr[0] = get_pixel(j * quad_s, i * quad_s, b);
					cr[1] = get_pixel(j * quad_s + qx - 1, i * quad_s, b);
					cr[2] = get_pixel(j * quad_s + qx - 1, i * quad_s + qy - 1, b);
					cr[3] = get_pixel(j * quad_s, i * quad_s + qy - 1, b);
				} else {
					rect areas[4];
					areas[0].x = j * quad_s;
					areas[1].x = j * quad_s + qx - (qx / 4);
					areas[2].x = j * quad_s + qx - (qx / 4);
					areas[3].x = j * quad_s;
					areas[0].w = qx / 4 - 1;
					areas[1].w = qx / 4 - 1;
					areas[2].w = qx / 4 - 1;
					areas[3].w = qx / 4 - 1;
					areas[0].y = i * quad_s;
					areas[1].y = i * quad_s;
					areas[2].y = i * quad_s + qy - (qy / 4);
					areas[3].y = i * quad_s + qy - (qy / 4);
					areas[0].h = qy / 4 - 1;
					areas[1].h = qy / 4 - 1;
					areas[2].h = qy / 4 - 1;
					areas[3].h = qy / 4 - 1;
					for (u32 k = 0; k < 4; k++) {
						if (areas[k].w == 0) {
							areas[k].w = 1;
						}
						if (areas[k].h == 0) {
							areas[k].h = 1;
						}
					}
					cr[0] = average_color(areas[0], b);
					cr[1] = average_color(areas[1], b);
					cr[2] = average_color(areas[2], b);
					cr[3] = average_color(areas[3], b);
				}
				dstoffsetcopy(ret.ptr, &cr[0], &offset, esize);
				dstoffsetcopy(ret.ptr, &cr[1], &offset, esize);
				dstoffsetcopy(ret.ptr, &cr[2], &offset, esize);
				dstoffsetcopy(ret.ptr, &cr[3], &offset, esize);
			} else {
				u32 simillar[3];
				memset(simillar, 0, sizeof(u32) * 3);
				for (u32 k = 0; k < qy; k++) {
					for (u32 l = 0; l < qx; l++) {
						union piexl_data dst_pix;
						dst_pix.pixel = get_pixel(j * quad_s + l, i * quad_s + k, b);
						union piexl_data up_pix;
						up_pix.pixel = get_pixel(j * quad_s + l, (i - 1) * quad_s + k, b);
						union piexl_data lef_pix;
						lef_pix.pixel = get_pixel((j - 1) * quad_s + l, i * quad_s + k, b);
						for (u32 c = 0; c < 3; c++) {
							u32 a_diff =
									abs((i32)dst_pix.channels[c] - (i32)up_pix.channels[c]);
							u32 b_diff =
									abs((i32)dst_pix.channels[c] - (i32)lef_pix.channels[c]);
							if (a_diff > b_diff) {
								simillar[c]++;
							}
						}
					}
				}
				for (u32 c = 0; c < 3; c++) {
					simillar[c] = simillar[c] * 255 / (qy * qx);
					u8 val = simillar[c];
					dstoffsetcopy(ret.ptr, &val, &offset, 1);
				}
			}
			u32 pos = i * (x_blocks_size) + j;
			blocks.ptr[pos / 4] = blocks.ptr[pos / 4] | (algo << ((pos % 4) * 2));
		}
	}

	ret.size = offset;
	// resize array to adjust to stripped data
	ret.ptr = realloc(ret.ptr, offset);
	return ret;
}
bitmap *recreate_quads(stream str, u8 quad_s, rect size, u8 format,
											 stream blokcs) {
	// translate form array to bitmap  and interpolate missing quds
	bitmap *ret = create_bitmap(size.w, size.h, size.w, format);
	// size of pixel
	u32 esize = format_bpp(format);
	// reallocating output stream to avoid memory safety issues if file is corrupted
	str.ptr = realloc(str.ptr, size.w * size.h * esize);
	// index of written last byte in stream
	u32 offset = 0;
	u32 x_blocks_size = ret->x / quad_s;
	u32 y_blocks_size = ret->y / quad_s;
	if (x_blocks_size == 0) {
		x_blocks_size = 1;
		quad_s = ret->x;
	}
	if (y_blocks_size == 0) {
		y_blocks_size = 1;
		quad_s = ret->y;
	}
	for (u32 i = 0; i < y_blocks_size; i++) {
		for (u32 j = 0; j < x_blocks_size; j++) {
			u32 cr[4];
			memset(cr, 0, sizeof(u32) * 4);
			u32 qx = quad_s;
			u32 qy = quad_s;

			if (i + 1 == ret->y / quad_s) {
				qy = ret->y - quad_s * i;
			}
			if (j + 1 == ret->x / quad_s) {
				qx = ret->x - quad_s * j;
			}

			// copying corners first

			// determining if difference is lesser or greater tahn thershold
			// algorithm used in block

			u16 pos = i * x_blocks_size + j;
			u32 bitshift = (pos % 4 * 2);
			u8 algo = (blokcs.ptr[pos / 4] & (3 << bitshift)) >> bitshift;

			if (algo == 1) {

				// block is  wasn't skipped and  other pixels are  copied
				rect quad_rect;
				quad_rect.x = quad_s * j;
				quad_rect.y = quad_s * i;
				quad_rect.w = qx;
				quad_rect.h = qy;
#if NOSUBSAMPLING
				defullsampling(ret, quad_rect, str, &offset);
#else
				desubsampling_yuv(ret, quad_rect, str, &offset);
#endif
			} else if (algo == 0) {

				srcoffsetcopy(&cr[0], str.ptr, &offset, esize);
				srcoffsetcopy(&cr[1], str.ptr, &offset, esize);
				srcoffsetcopy(&cr[2], str.ptr, &offset, esize);
				srcoffsetcopy(&cr[3], str.ptr, &offset, esize);
				set_pixel(j * quad_s, i * quad_s, ret, cr[0]);
				set_pixel(j * quad_s + qx - 1, i * quad_s, ret, cr[1]);
				set_pixel(j * quad_s + qx - 1, i * quad_s + qy - 1, ret, cr[2]);
				set_pixel(j * quad_s, i * quad_s + qy - 1, ret, cr[3]);
				// block was skipped and   pixels are interpolated
				rgba_color *corners = (rgba_color *)cr;
				for (u32 k = 1; k < qx - 1; k++) {
					// intrpolating horizontally upper edge
					float dist = (float)k / (qx - 1);
					rgba_color pixel;
					rgba_color pixelb = *(rgba_color *)&cr[0];
					rgba_color pixela = *(rgba_color *)&cr[1];
					pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
					pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
					pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
					if (esize == 4) {
						pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
					}
					set_pixel(quad_s * j + k, quad_s * i, ret, *(u32 *)&pixel);
				}
				// intrpolating horizontally lower edge
				for (u32 k = 1; k < qx - 1; k++) {
					float dist = (float)k / (qx - 1);
					rgba_color pixel;
					rgba_color pixelb = *(rgba_color *)&cr[3];
					rgba_color pixela = *(rgba_color *)&cr[2];
					pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
					pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
					pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
					if (esize == 4) {
						pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
					}
					set_pixel(quad_s * j + k, quad_s * i + qy - 1, ret, *(u32 *)&pixel);
				}
				// interpolating vertically each column
				for (u32 k = 0; k < qx; k++) {
					u32 cola = get_pixel(quad_s * j + k, quad_s * i, ret);
					u32 colb = get_pixel(quad_s * j + k, quad_s * i + qy - 1, ret);
					rgba_color pixelb = *(rgba_color *)&cola;
					rgba_color pixela = *(rgba_color *)&colb;
					for (u32 l = 1; l < qy - 1; l++) {

						float dist = (float)l / (qy - 1);
						rgba_color pixel;

						pixel.r = roundf(pixela.r * dist + pixelb.r * (1.0f - dist));
						pixel.g = roundf(pixela.g * dist + pixelb.g * (1.0f - dist));
						pixel.b = roundf(pixela.b * dist + pixelb.b * (1.0f - dist));
						if (esize == 4) {
							pixel.a = roundf(pixela.a * dist + pixelb.a * (1.0f - dist));
						}
						set_pixel(quad_s * j + k, quad_s * i + l, ret, *(u32 *)&pixel);
					}
				}
			} else {
				u8 weight[3];
				srcoffsetcopy(weight, str.ptr, &offset, 3);
				for (u32 k = 0; k < qy; k++) {
					for (u32 l = 0; l < qx; l++) {
						union piexl_data a_pixel;
						union piexl_data b_pixel;
						a_pixel.pixel =
								get_pixel(quad_s * (j - 1) + l, quad_s * i + k, ret);
						b_pixel.pixel =
								get_pixel(quad_s * j + l, quad_s * (i - 1) + k, ret);
						union piexl_data out_pixel;
						for (u32 c = 0; c < 3; c++) {
							u32 color = (a_pixel.channels[c] * (u32)weight[c] +
													 b_pixel.channels[c] * (u32)(255 - weight[c])) /
													255;
							out_pixel.channels[c] = (u8)color;
						}
						set_pixel(quad_s * j + l, quad_s * i + k, ret, out_pixel.pixel);
					}
				}
			}
#if SHOW_BLOCKS == 1
			u32 color;
			if (algo == 0) {
				color = 0x80FF0000;
			} else if (algo == 1) {
				color = 0x0000FF00;
			} else {
				color = 0x00FFFF00;
			}
			for (u32 k = 0; k < qx; k++) {
				set_pixel(quad_s * j + k, quad_s * i + qy - 1, ret, color);
			}
			for (u32 k = 0; k < qy; k++) {
				set_pixel(quad_s * j + qx - 1, quad_s * i + k, ret, color);
			}

#endif
		}
	}
	free(str.ptr);
	return ret;
}
void free_image(image *img) {
	for (u32 i = 0; i < img->length; i++) {
		free(img->parts[i].pixels.ptr);
		free(img->parts[i].blokcs.ptr);
	}
	free(img->parts);
	free(img->dicts);

	free(img);
}
u8 merge_dicts(dict8 *dst, dict8 *src, stream *pixels) {
	u8 matches = 0;
	u8 in[256];
	u8 out[256];
	u8 changed[256];
	for (u32 i = 0; i < 256; i++) {
		changed[i] = 0;
	}
	for (u32 i = 0; i < dst->size; i++) {
		for (u32 j = 0; j < src->size; j++) {

			if (dst->colors[i] == src->colors[j]) {
				in[matches] = j;
				out[matches] = i;
				changed[j] = 1;
				matches++;
			}
		}
	}
	if ((i32)dst->size + (i32)src->size - matches >= 255) {
		return 0;
	} else {
		for (u32 i = 0; i < (u32)src->size; i++) {
			if (changed[i] == 0) {
				dst->colors[dst->size] = src->colors[i];
				in[matches] = i;
				out[matches] = dst->size;
				changed[i] = 1;
				matches++;
				dst->size++;
			}
		}
	}
	for (u32 i = 0; i < pixels->size; i++) {
		if (changed[pixels->ptr[i]] == 1) {
			for (u32 j = 0; j < matches; j++) {
				if (pixels->ptr[i] == in[j]) {
					pixels->ptr[i] = out[j];
					break;
				}
			}
		}
	}

	return 1;
}
u32 average_color(rect area, bitmap *b) {
	u32 avg[4];
	memset(avg, 0, sizeof(u32) * 4);
	u8 esize = format_bpp(b->format);
	for (u32 i = area.y; i < area.y + area.h; i++) {
		for (u32 j = area.x; j < area.x + area.w; j++) {
			u32 pixel = get_pixel(j, i, b);
			for (u32 k = 0; k < esize; k++) {
				u8 *channel = (u8 *)&pixel;
				avg[k] += *(channel + k);
			}
		}
	}
	u32 res_avg = 0;
	for (u32 k = 0; k < esize; k++) {
		*(((u8 *)&res_avg) + k) = roundf(avg[k] / (float)(area.w * area.h));
	}
	return res_avg;
}
bitmap *rgb_to_yuv(bitmap *b) {
	bitmap *ret = create_bitmap(b->x, b->y, b->x, YUV444);
	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			u32 yuv = 0;
			u32 rgb = get_pixel(j, i, b);
			u8 *yuv_channel = (u8 *)&yuv;
			u8 *rgb_channel = (u8 *)&rgb;
			yuv_channel[0] = RGB2Y(rgb_channel[0], rgb_channel[1], rgb_channel[2]);
			yuv_channel[1] = RGB2U(rgb_channel[0], rgb_channel[1], rgb_channel[2]);
			yuv_channel[2] = RGB2V(rgb_channel[0], rgb_channel[1], rgb_channel[2]);

			set_pixel(j, i, ret, yuv);
		}
	}
	return ret;
}
bitmap *yuv_to_rgb(bitmap *b) {
	bitmap *ret = create_bitmap(b->x, b->y, b->x, RGB24);
	for (u32 i = 0; i < b->y; i++) {
		for (u32 j = 0; j < b->x; j++) {
			u32 rgb = 0;
			u32 yuv = get_pixel(j, i, b);
			u8 *yuv_channel = (u8 *)&yuv;
			u8 *rgb_channel = (u8 *)&rgb;
			rgb_channel[0] = YUV2R(yuv_channel[0], yuv_channel[1], yuv_channel[2]);
			rgb_channel[1] = YUV2G(yuv_channel[0], yuv_channel[1], yuv_channel[2]);
			rgb_channel[2] = YUV2B(yuv_channel[0], yuv_channel[1], yuv_channel[2]);
			set_pixel(j, i, ret, rgb);
		}
	}
	return ret;
}
void edeges_map(image *img, bitmap *yuv) {
	stream edges;
	u64 all_edges = 0;
	edges.size = yuv->x * yuv->y;
	edges.ptr = malloc(edges.size);
	for (u32 i = 0; i < yuv->y; i++) {
		for (u32 j = 0; j < yuv->x; j++) {
			u32 edge = edge_detection_yuv(yuv, j, i);
			all_edges += edge;
			edges.ptr[i * yuv->x + j] = edge;
		}
	}
	img->edges_map = edges;
	img->avg_edges = all_edges / (yuv->x * yuv->y);
}
void subsampling_yuv(bitmap *b, rect area, stream out, u32 *offset) {
	for (u32 i = area.y; i < ((area.h + area.y) / 2) * 2; i += 2) {
		for (u32 j = area.x; j < ((area.w + area.x) / 2) * 2; j += 2) {

			u32 u_sample = 0;
			u32 v_sample = 0;
			u16 samples_count = 0;
			for (u32 k = 0; k < 2; k++) {
				for (u32 l = 0; l < 2; l++) {

					union piexl_data px;
					px.pixel = get_pixel(j, i, b);
					dstoffsetcopy(out.ptr, &px.yuva.y, offset, sizeof(u8));
					u_sample += px.yuva.u;
					v_sample += px.yuva.v;
					samples_count++;
				}
			}

			u_sample /= 4;
			v_sample /= 4;
			u8 u_val = u_sample;
			u8 v_val = v_sample;
			dstoffsetcopy(out.ptr, &u_sample, offset, sizeof(u8));
			dstoffsetcopy(out.ptr, &v_sample, offset, sizeof(u8));
		}
	}
	if ((area.h) % 2) {
		for (u32 i = area.x; i < area.x + area.w - 1; i++) {
			union piexl_data px;
			px.pixel = get_pixel(i, area.y + area.h - 1, b);
			dstoffsetcopy(out.ptr, &px.pixel, offset, 3);
		}
	}
	if ((area.w) % 2) {
		for (u32 i = area.y; i < area.h + area.y - 1; i++) {
			union piexl_data px;
			px.pixel = get_pixel(area.x + area.w - 1, i, b);
			dstoffsetcopy(out.ptr, &px.pixel, offset, 3);
		}
	}
	if (((area.h) % 2) || ((area.w) % 2)) {
		union piexl_data px;
		px.pixel = get_pixel(area.x + area.w - 1, area.y + area.h - 1, b);
		dstoffsetcopy(out.ptr, &px.pixel, offset, 3);
	}
}
void desubsampling_yuv(bitmap *b, rect area, stream str, u32 *offset) {
	for (u32 i = area.y; i < ((area.h + area.y) / 2) * 2; i += 2) {
		for (u32 j = area.x; j < ((area.w + area.x) / 2) * 2; j += 2) {
			union piexl_data px;

			u8 y_val[4];
			srcoffsetcopy(y_val, str.ptr, offset, sizeof(u8) * 4);
			srcoffsetcopy(&px.yuva.u, str.ptr, offset, sizeof(u8));
			srcoffsetcopy(&px.yuva.v, str.ptr, offset, sizeof(u8));

			for (u32 k = 0; k < 2; k++) {
				for (u32 l = 0; l < 2; l++) {
					px.yuva.y = y_val[k * 2 + l];
					set_pixel(j + l, i + k, b, px.pixel);
				}
			}
		}
	}
	if ((area.h) % 2) {
		for (u32 i = area.x; i < area.x + area.w - 1; i++) {
			union piexl_data px;
			srcoffsetcopy(&px.pixel, str.ptr, offset, 3);
			set_pixel(i, area.y + area.h - 1, b, px.pixel);
		}
	}
	if ((area.w) % 2) {
		for (u32 i = area.y; i < area.h + area.y - 1; i++) {
			union piexl_data px;
			srcoffsetcopy(&px.pixel, str.ptr, offset, 3);
			set_pixel(area.w + area.x - 1, i, b, px.pixel);
		}
	}
	if (((area.h) % 2) || ((area.w) % 2)) {
		union piexl_data px;
		srcoffsetcopy(&px.pixel, str.ptr, offset, 3);
		set_pixel(area.w + area.x - 1, area.y + area.h - 1, b, px.pixel);
	}
}
void fullsampling(bitmap *b, rect area, stream out, u32 *offset) {
	u8 esize = format_bpp(b->format);
	for (u32 i = area.y; i < area.h + area.y; i++) {
		for (u32 j = area.x; j < area.w + area.x; j++) {
			u32 pix = get_pixel(j, i, b);
			dstoffsetcopy(out.ptr, &pix, offset, esize);
		}
	}
}
void defullsampling(bitmap *b, rect area, stream str, u32 *offset) {
	u8 esize = format_bpp(b->format);
	for (u32 i = area.y; i < area.h + area.y; i++) {
		for (u32 j = area.x; j < area.w + area.x; j++) {
			u32 pix;
			srcoffsetcopy(&pix, str.ptr, offset, esize);
			set_pixel(j, i, b, pix);
		}
	}
}
