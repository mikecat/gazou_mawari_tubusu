#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "image_io.h"

int main(int argc, char* argv[]) {
	image_t *img, *next_img, *tmp;
	uint32_t x, y;
	int progress, invalid_found;
	if (argc < 4) {
		fprintf(stderr, "Usage: %s input_img mask_img output_img\n",
			argc > 0 ? argv[0] : "gazou_mawari_tubusu");
		return 1;
	}
	img = load_image(argv[1]);
	next_img = load_image(argv[2]);
	if (img == NULL || next_img == NULL) {
		fprintf(stderr, "failed to load image(s)\n");
		free_image(img);
		free_image(next_img);
		return 1;
	}
	if (img->width != next_img->width || img->height != next_img->height) {
		fprintf(stderr, "size of input and mask image mismatch\n");
		free_image(img);
		free_image(next_img);
		return 1;
	}
	for (y = 0; y < img->height; y++) {
		for (x = 0; x < img->width; x++) {
			rgb_t mask_pixel = next_img->image[y * next_img->width + x];
			img->image[y * img->width + x].userdata =
				(mask_pixel.r + mask_pixel.g + mask_pixel.b >= 128 * 3);
		}
	}
	do {
		progress = 0;
		invalid_found = 0;
		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++) {
				if (img->image[y * img->width + x].userdata) {
					next_img->image[y * next_img->width + x] = img->image[y * img->width + x];
				} else {
					int dy, dx;
					int valid_count = 0, r = 0, g = 0, b = 0;
					rgb_t next_px;
					for (dy = -1; dy <= 1; dy++) {
						if ((y == 0 && dy < 0) || y + dy >= img->height) continue;
						for (dx = -1; dx <= 1; dx++) {
							rgb_t px;
							if ((x == 0 && dx < 0) || x + dx >= img->width) continue;
							if ((px = img->image[(y + dy) * img->width + (x + dx)]).userdata) {
								valid_count++;
								r += px.r;
								g += px.g;
								b += px.b;
							}
						}
					}
					if (valid_count >= 3) {
						next_px.r = r / valid_count;
						next_px.g = g / valid_count;
						next_px.b = b / valid_count;
						next_px.userdata = 1;
						progress = 1;
					} else {
						next_px.r = next_px.g = next_px.b = next_px.userdata = 0;
						invalid_found = 1;
					}
					next_img->image[y * next_img->width + x] = next_px;
				}
			}
		}
		tmp = img;
		img = next_img;
		next_img = tmp;
	} while (progress);
	if (invalid_found) {
		fprintf(stderr, "warning: there are some invalid pixels left!\n");
	}
	if (!save_image(argv[3], img)) {
		fprintf(stderr, "failed to save image\n");
	}
	free_image(img);
	free_image(next_img);
	return 0;
}
