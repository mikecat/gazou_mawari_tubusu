#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "image_io.h"

typedef struct {
	uint32_t x, y;
	rgb_t next_color;
} work_t;

int get_valid_colors(const image_t* img, rgb_t* average, uint32_t x, uint32_t y, int size) {
	int count = 0, r = 0, g = 0, b = 0;
	int dx, dy;
	for (dy = -size; dy <= size; dy++) {
		if ((dy < 0 && y < (unsigned int)-dy) || (img->height <= y + dy)) continue;
		for (dx = -size; dx <= size; dx++) {
			rgb_t px;
			if ((dx < 0 && x < (unsigned int)-dx) || (img->width <= x + dx)) continue;
			if ((px = img->image[(y + dy) * img->width + (x + dx)]).userdata) {
				count++;
				r += px.r;
				g += px.g;
				b += px.b;
			}
		}
	}
	if (average != NULL) {
		if (count > 0) {
			average->r = r / count;
			average->g = g / count;
			average->b = b / count;
			average->userdata = 1;
		} else {
			average->r = average->g = average->b = average->userdata = 0;
		}
	}
	return count;
}

int main(int argc, char* argv[]) {
	image_t *img, *mask_img;
	work_t *current_work, *next_work, *tmp;
	size_t current_work_size, next_work_size, i;
	uint32_t x, y;
	int invalid_found;
	int correct_size = 1;
	if (argc < 4) {
		fprintf(stderr, "Usage: %s input_img mask_img output_img [correct_size]\n",
			argc > 0 ? argv[0] : "gazou_mawari_tubusu");
		return 1;
	}
	if (argc >= 5) {
		if ((correct_size = atoi(argv[4])) <= 0) {
			fprintf(stderr, "correct_size must be positive\n");
			return 1;
		}
	}
	img = load_image(argv[1]);
	mask_img = load_image(argv[2]);
	if (img == NULL || mask_img == NULL) {
		fprintf(stderr, "failed to load image(s)\n");
		free_image(img);
		free_image(mask_img);
		return 1;
	}
	if (img->width != mask_img->width || img->height != mask_img->height) {
		fprintf(stderr, "size of input and mask image mismatch\n");
		free_image(img);
		free_image(mask_img);
		return 1;
	}
	if ((current_work = malloc(sizeof(work_t) * img->width * img->height)) == NULL ||
	(next_work = malloc(sizeof(work_t) * img->width * img->height)) == NULL) {
		fprintf(stderr, "work buffer allocate failed\n");
		free_image(img);
		free_image(mask_img);
		free(current_work);
		free(next_work);
		return 1;
	}
	#ifdef _OPENMP
	#pragma omp parallel for collapse(2)
	#endif
	for (y = 0; y < img->height; y++) {
		for (x = 0; x < img->width; x++) {
			rgb_t mask_pixel = mask_img->image[y * mask_img->width + x];
			mask_img->image[y * mask_img->width + x].userdata =
				img->image[y * img->width + x].userdata =
					(mask_pixel.r + mask_pixel.g + mask_pixel.b >= 128 * 3);
		}
	}
	next_work_size = 0;
	#ifdef _OPENMP
	#pragma omp parallel for collapse(2)
	#endif
	for (y = 0; y < img->height; y++) {
		for (x = 0; x < img->width; x++) {
			if (!mask_img->image[y * mask_img->width + x].userdata &&
			get_valid_colors(img, NULL, x, y, correct_size) > 0) {
				size_t work_index;
				mask_img->image[y * mask_img->width + x].userdata = 1;
				#ifdef _OPENMP
				#pragma omp critical
				#endif
				{
					work_index = next_work_size++;
				}
				next_work[work_index].x = x;
				next_work[work_index].y = y;
			}
		}
	}
	while (next_work_size > 0) {
		tmp = current_work;
		current_work = next_work;
		next_work = tmp;
		current_work_size = next_work_size;
		next_work_size = 0;
		#ifdef _OPENMP
		#pragma omp parallel
		#endif
		{
			#ifdef _OPENMP
			#pragma omp for
			#endif
			for (i = 0; i < current_work_size; i++) {
				get_valid_colors(img, &current_work[i].next_color,
					current_work[i].x, current_work[i].y, correct_size);
			}
			#ifdef _OPENMP
			#pragma omp for
			#endif
			for (i = 0; i < current_work_size; i++) {
				img->image[current_work[i].y * img->width + current_work[i].x] = current_work[i].next_color;
			}
			#ifdef _OPENMP
			#pragma omp for private(x, y)
			#endif
			for (i = 0; i < current_work_size; i++) {
				int dx, dy;
				x = current_work[i].x;
				y = current_work[i].y;
				for (dy = -1; dy <= 1; dy++) {
					if ((dy < 0 && y < (unsigned int)-dy) || img->height <= y + dy) continue;
					for (dx = -1; dx <= 1; dx++) {
						if ((dx < 0 && x < (unsigned int)-dx) || img->width <= x + dx) continue;
						if (!mask_img->image[(y + dy) * mask_img->width + (x + dx)].userdata &&
						get_valid_colors(img, NULL, x + dx, y + dy, correct_size) > 0) {
							size_t work_index;
							mask_img->image[(y + dy) * mask_img->width + (x + dx)].userdata = 1;
							#ifdef _OPENMP
							#pragma omp critical
							#endif
							{
								work_index = next_work_size++;
							}
							next_work[work_index].x = x + dx;
							next_work[work_index].y = y + dy;
						}
					}
				}
			}
		}
	}
	invalid_found = 0;
	#ifdef _OPENMP
	#pragma omp parallel for collapse(2)
	#endif
	for (y = 0; y < img->height; y++) {
		for (x = 0; x < img->width; x++) {
			if (!img->image[y * img->width + x].userdata) invalid_found = 1;
		}
	}
	if (invalid_found) {
		fprintf(stderr, "warning: there are some invalid pixels left!\n");
	}
	if (!save_image(argv[3], img)) {
		fprintf(stderr, "failed to save image\n");
	}
	free_image(img);
	free_image(mask_img);
	free(current_work);
	free(next_work);
	return 0;
}
