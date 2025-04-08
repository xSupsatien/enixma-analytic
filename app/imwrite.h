#pragma once

#include <stdbool.h>
#include <jansson.h>

#define MAX_IMAGES 10

extern char *incident_images[MAX_IMAGES];
extern int incident_images_size;

void imwrite(char *filename, void *img);
bool save_image_name(const char* filename, char** image_names, int array_size);
bool load_image_name(const char* filename, char** image_names, int array_size);
void cleanup_incident_images_directory(void);