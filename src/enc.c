#include "image.h"
#include "iofile.h"
#include "types.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
	char *import_path = NULL;
	char *export_path = NULL;
	encode_args args;
	args.encode_method = 0;
	args.block_color_sensitivity = 30;
	args.color_reduction = 0;
	args.complexity = 2;
	args.dct_quant_max = 2048;
	args.dct_quant_min = 4;

	for (uint16_t i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			printf(
					"encodes image (webp/png/jpg/gif) into .kczr image\n"
					"enc  -i <input> -o <output>\n"
					"options:\n -h    help\n -o    output filename.kczi\n "
					"-b "
					"<number>"
					"  size of bloks in px \n -q <number>   color reduction  "
					"(none)1-128(two colors)\n"
					"-s blocks difference sensitivity \n");
			return 0;
		} else if (strcmp(argv[i], "-o") == 0) {
			export_path = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "-i") == 0) {
			import_path = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "-q") == 0) {
			args.color_reduction = atoi(argv[i + 1]);
			if (args.color_reduction > 255 || args.color_reduction < 0) {
				printf("inncorect value -q %i \n", args.color_reduction);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-b") == 0) {
			args.max_block_size = atoi(argv[i + 1]);
			if (args.max_block_size > 500 || args.max_block_size < 1) {
				printf("inncorect value -b %i \n", args.max_block_size);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-s") == 0) {
			args.block_color_sensitivity = atoi(argv[i + 1]);

			if (args.block_color_sensitivity > 500 || args.block_color_sensitivity < 0) {
				printf("inncorect value -s %i \n", args.block_color_sensitivity);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-m") == 0) {
			args.encode_method = atoi(argv[i + 1]);

			if (args.encode_method > 3 || args.encode_method < 0) {
				printf("inncorect value -m %i \n", args.encode_method);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-c") == 0) {
			args.complexity = atoi(argv[i + 1]);

			if (args.complexity > 2 || args.complexity < 0) {
				printf("inncorect value -c %i \n", args.encode_method);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "--dct-min") == 0) {
			args.dct_quant_min = atoi(argv[i + 1]);

			if (args.dct_quant_min > 8192 || args.dct_quant_min < 0) {
				printf("inncorect value --dct-min %i \n", args.encode_method);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "--dct-max") == 0) {
			args.dct_quant_max = atoi(argv[i + 1]);

			if (args.dct_quant_max > 16384 || args.dct_quant_max < 0) {
				printf("inncorect value --dct-max %i \n", args.dct_quant_max);
				return 0;
			}
			i++;
		} else if (argv[i][0] == '-') {
			printf("incorrect switch %s", argv[i]);
			return 0;
		}
	}
	if (export_path == NULL) {
		printf("usage: enc -i <input> -o <output>\n -h help\n");
		return 0;
	}
	if (import_path == NULL) {
		printf("usage: enc -i <input> -o <output>\n -h help\n");
		return 0;
	}
	SDL_Surface *surf = IMG_Load(import_path);
	bitmap *b = sdl_to_bitmap(surf);

	image *img = encode(b, args);
	u64 decompressed_size;
	stream str = seralize(img);
	write(&str, export_path);
	free(str.ptr);
	free_bitmap(b);
	// free_image(img);
	SDL_FreeSurface(surf);
	return 0;
}
