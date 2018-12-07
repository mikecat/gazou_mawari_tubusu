#ifndef IMAGE_IO_H_GUARD_5544C91E_DFD3_40A6_9073_2F6E446E4479
#define IMAGE_IO_H_GUARD_5544C91E_DFD3_40A6_9073_2F6E446E4479

#include <stdint.h>

typedef struct {
	uint8_t r, g, b;
	uint8_t userdata;
} rgb_t;

typedef struct {
	uint32_t width, height;
	rgb_t* image;
} image_t;

image_t* create_image(uint32_t width, uint32_t height);
image_t* load_image(const char* filename);
int save_image(const char* filename, const image_t* img);
void free_image(image_t* img);

#endif
