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
	u32 qu = 4;
	u32 bl = 16;
	u32 se = 28;

	for (uint16_t i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			printf(
					"encodes image (webp/png/jpg/gif) into .kczr image\n enc  -i <input> "
					"-o <output>\n options:\n -h    help\n -o    output filename.kczi\n "
					"-b "
					"<number>"
					"  size of bloks in px \n -q <number>   color reduction  "
					"(none)1-128(two colors)\n"
					"-s blocks diffrence sensivity \n");
			return 0;
		} else if (strcmp(argv[i], "-o") == 0) {
			export_path = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "-i") == 0) {
			import_path = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "-q") == 0) {
			qu = atoi(argv[i + 1]);

			if (qu > 255 || qu < 0) {
				printf("inncorect value -q %i \n", qu);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-b") == 0) {

			bl = atoi(argv[i + 1]);
			if (bl > 500 || bl < 1) {
				printf("inncorect value -b %i \n", bl);
				return 0;
			}
			i++;
		} else if (strcmp(argv[i], "-s") == 0) {
			se = atoi(argv[i + 1]);

			if (se > 5000 || se < 0) {
				printf("inncorect value -s %i \n", se);
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
	image *img = encode(b, bl, qu, se, 2);
	u64 decompressed_size;
	stream str = seralize(img);
	write(&str, export_path);
	free(str.ptr);
	free_bitmap(b);
	// free_image(img);
	SDL_FreeSurface(surf);
	return 0;
}
