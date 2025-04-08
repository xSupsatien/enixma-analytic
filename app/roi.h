#pragma once

#include <stdbool.h>
#include <stdlib.h>

#define MAX_POLYGON_POINTS 100

typedef struct {
    float x;
    float y;
} PolygonPoint;

typedef struct {
    PolygonPoint* points;  // Array of points defining the polygon
    int num_points;        // Number of points in the polygon
} Polygon;

// Global variable declaration
extern Polygon* roi1;
extern Polygon* roi2;

// Function declarations
Polygon* init_polygon(int max_points);
bool add_polygon_point(Polygon* poly, float x, float y);
bool is_point_in_polygon(float x, float y, Polygon* poly);
bool is_in_roi(float* bbox, Polygon* roi);
void free_polygon(Polygon* poly);