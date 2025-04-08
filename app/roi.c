#include "roi.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

Polygon* roi1 = NULL;
Polygon* roi2 = NULL;

// Initialize polygon
Polygon* init_polygon(int max_points) {
    Polygon* poly = (Polygon*)malloc(sizeof(Polygon));
    if (!poly) return NULL;

    poly->points = (PolygonPoint*)malloc(max_points * sizeof(PolygonPoint));
    if (!poly->points) {
        free(poly);
        return NULL;
    }

    poly->num_points = 0;
    return poly;
}

// Add point to polygon
bool add_polygon_point(Polygon* poly, float x, float y) {
    if (!poly || poly->num_points >= MAX_POLYGON_POINTS) return false;
    
    poly->points[poly->num_points].x = x;
    poly->points[poly->num_points].y = y;
    poly->num_points++;
    return true;
}

// Check if point is inside polygon using ray casting algorithm
bool is_point_in_polygon(float x, float y, Polygon* poly) {
    if (!poly || poly->num_points < 3) return false;

    bool inside = false;
    for (int i = 0, j = poly->num_points - 1; i < poly->num_points; j = i++) {
        if (((poly->points[i].y > y) != (poly->points[j].y > y)) &&
            (x < (poly->points[j].x - poly->points[i].x) * (y - poly->points[i].y) /
                 (poly->points[j].y - poly->points[i].y) + poly->points[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Check if object's center point is in ROI
bool is_in_roi(float* bbox, Polygon* roi) {
    float center_x = (bbox[1] + bbox[3]) / 2.0f;  // (left + right) / 2
    float center_y = (bbox[0] + bbox[2]) / 2.0f;  // (top + bottom) / 2
    return is_point_in_polygon(center_x, center_y, roi);
}

// Free polygon resources
void free_polygon(Polygon* poly) {
    if (poly) {
        if (poly->points) {
            free(poly->points);
        }
        free(poly);
    }
}