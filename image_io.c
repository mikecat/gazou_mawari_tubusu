#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <png.h>
#include <jpeglib.h>
#include "image_io.h"

image_t* create_image(uint32_t width, uint32_t height) {
	image_t* img;
	if (width <= 0 || height <= 0) return NULL;
	if ((img = malloc(sizeof(*img))) == NULL) {
		return NULL;
	}
	img->width = width;
	img->height = height;
	if (SIZE_MAX / sizeof(*(img->image)) < img->width ||
	SIZE_MAX / (sizeof(*(img->image)) * img->width) < img->height ||
	(img->image = malloc(sizeof(*(img->image)) * img->width * img->height)) == NULL) {
		free(img);
		return NULL;
	}
	return img;
}

image_t* load_image(const char* filename) {
	FILE* fp;
	png_byte png_header[8];
	if (filename == NULL) return NULL;
	if ((fp = fopen(filename, "rb")) == NULL) return NULL;
	if (fread(png_header, sizeof(png_byte), 8, fp) == 8 && png_sig_cmp(png_header, 0, 8) == 0) {
		/* seems PNG file */
		png_structp png_ptr;
		png_infop info_ptr;
		png_bytepp rows;
		image_t* img = NULL;
		uint32_t x, y;
		if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
			fclose(fp);
			return NULL;
		}
		if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
			png_destroy_read_struct(&png_ptr, NULL, NULL);
			fclose(fp);
			return NULL;
		}
		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			fclose(fp);
			free_image(img);
			return NULL;
		}
		png_init_io(png_ptr, fp);
		png_set_sig_bytes(png_ptr, 8);
		png_read_png(png_ptr, info_ptr,
			PNG_TRANSFORM_SCALE_16 | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING |
			PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_SHIFT | PNG_TRANSFORM_GRAY_TO_RGB, NULL);
		rows = png_get_rows(png_ptr, info_ptr);
		if ((img = create_image(png_get_image_width(png_ptr, info_ptr),
		png_get_image_height(png_ptr, info_ptr))) == NULL) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			fclose(fp);
			return NULL;
		}
		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++) {
				img->image[y * img->width + x].r = rows[y][x * 3 + 0];
				img->image[y * img->width + x].g = rows[y][x * 3 + 1];
				img->image[y * img->width + x].b = rows[y][x * 3 + 2];
			}
		}
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return img;
	} else {
		/* not PNG, assume as JPEG */
		struct jpeg_decompress_struct cinfo;
		struct jpeg_error_mgr jerr;
		JSAMPROW row;
		image_t* img = NULL;
		uint32_t x, y;
		fseek(fp, 0, SEEK_SET);
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);
		jpeg_stdio_src(&cinfo, fp);
		jpeg_read_header(&cinfo, TRUE);
		cinfo.out_color_space = JCS_RGB;
		jpeg_start_decompress(&cinfo);
		if ((img = create_image(cinfo.output_width, cinfo.output_height)) == NULL) {
			jpeg_destroy_decompress(&cinfo);
			fclose(fp);
			return NULL;
		}
		if ((row = malloc(sizeof(*row) * cinfo.output_width * cinfo.output_components)) == NULL) {
			jpeg_destroy_decompress(&cinfo);
			fclose(fp);
			free_image(img);
			return NULL;
		}
		for (y = 0; y < img->height; y++) {
			jpeg_read_scanlines(&cinfo, &row, 1);
			for (x = 0; x < img->width; x++) {
				img->image[y * img->width + x].r = row[x * 3 + 0];
				img->image[y * img->width + x].g = row[x * 3 + 1];
				img->image[y * img->width + x].b = row[x * 3 + 2];
			}
		}
		jpeg_destroy_decompress(&cinfo);
		free(row);
		fclose(fp);
		return img;
	}
}

int save_image(const char* filename, const image_t* img) {
	char extbuf[16];
	char* rdot;
	if (filename == NULL || img == NULL || img->image == NULL) return 0;
	rdot = strrchr(filename, '.');
	if (rdot != NULL) {
		int i;
		strncpy(extbuf, rdot, sizeof(extbuf));
		extbuf[sizeof(extbuf) / sizeof(*extbuf) - 1] = '\0';
		for (i = 0; extbuf[i] != '\0'; i++) {
			extbuf[i] = tolower((unsigned char)extbuf[i]);
		}
	} else {
		extbuf[0] = '\0';
	}
	if (strcmp(extbuf, ".jpg") == 0 || strcmp(extbuf, ".jpeg") == 0) {
		/* save in JPEG */
		FILE* fp;
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		JSAMPROW row;
		uint32_t x, y;
		if ((row = malloc(sizeof(*row) * 3 * img->width)) == NULL) return 0;
		if ((fp = fopen(filename, "wb")) == NULL) {
			free(row);
			return 0;
		}
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);
		jpeg_stdio_dest(&cinfo, fp);
		cinfo.image_width = img->width;
		cinfo.image_height = img->height;
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo, 95, TRUE);
		jpeg_start_compress(&cinfo, TRUE);
		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++) {
				row[x * 3 + 0] = img->image[y * img->width + x].r;
				row[x * 3 + 1] = img->image[y * img->width + x].g;
				row[x * 3 + 2] = img->image[y * img->width + x].b;
			}
			jpeg_write_scanlines(&cinfo, &row, 1);
		}
		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);
		free(row);
		fclose(fp);
		return 1;
	} else {
		/* save in PNG by default */
		FILE* fp;
		png_structp png_ptr;
		png_infop info_ptr;
		png_bytep row;
		uint32_t x, y;
		if ((row = malloc(sizeof(*row) * img->width * 3)) == NULL) {
			return 0;
		}
		if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
			free(row);
			return 0;
		}
		if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
			png_destroy_write_struct(&png_ptr, NULL);
			free(row);
			return 0;
		}
		if ((fp = fopen(filename, "wb")) == NULL) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			free(row);
			return 0;
		}
		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			free(row);
			return 0;
		}
		png_init_io(png_ptr, fp);
		png_set_IHDR(png_ptr, info_ptr, img->width, img->height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png_ptr, info_ptr);
		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++) {
				row[x * 3 + 0] = img->image[y * img->width + x].r;
				row[x * 3 + 1] = img->image[y * img->width + x].g;
				row[x * 3 + 2] = img->image[y * img->width + x].b;
			}
			png_write_row(png_ptr, row);
		}
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		free(row);
		return 1;
	}
}

void free_image(image_t* img) {
	if (img != NULL) {
		free(img->image);
		free(img);
	}
}
