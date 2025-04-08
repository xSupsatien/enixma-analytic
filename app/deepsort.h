#pragma once

#include <math.h>
#include <time.h>
#include <stdbool.h>

#include "roi.h"

#define MAX_TRAJECTORY_POINTS 100  // Maximum points to store per trajectory
#define EPSILON 1e-6 // Define a small value for floating-point comparison
#define MAX_TRACK_ID 10000

typedef struct {
    float x;
    float y;
} Point;

// Forward declaration without causing conflicts with incident.h
#ifndef EVENT_TYPE_ENUM_DEFINED
typedef enum EventTypeEnum EventTypeEnum;
#endif

// Structure to represent a detected object
typedef struct {
    float bbox[4];  // [top, left, bottom, right]
    float score;
    int class_id;
    int track_id;   // Unique tracking ID
    float velocity[2];  // [dx, dy] for motion estimation
    float speed_kmh;    // Speed in km/hr
    int age;        // Number of frames this object has been tracked
    int hits;       // Number of detections associated with this track
    int time_since_update;  // Frames since last detection
    Point trajectory[MAX_TRAJECTORY_POINTS];
    int trajectory_count;
    bool counted;   // Flag for crossing line
    
    // Timer-related fields
    time_t start_time;     // When the object was first detected
    bool timer_active;     // Whether timer is running for this object
    
    // Event detection fields
    bool event_check_initialized;  // Whether the 15-second check has started
    time_t event_check_start;      // When the 15-second check started
    bool event_detected;           // Whether an event has been detected for this object
} TrackedObject;

// Structure for tracking history
typedef struct {
    TrackedObject* objects;
    int count;
    int capacity;
    float iou_threshold;
    int max_age;
    int min_hits;
    int next_track_id;  // Counter for generating unique track IDs
} Tracker;

// Global variable declaration
extern Tracker* tracker;
extern float frame_time;

// Function declarations - core tracking functions
void update_velocity(TrackedObject* obj, float frame_time, float pixels_per_meter, int widthFrameHD, int heightFrameHD);
Tracker* init_tracker(int capacity, float iou_threshold, int max_age, int min_hits);
float calculate_iou(float* box1, float* box2);
float calculate_speed_kmh(float dx, float dy, float frame_time, float pixels_per_meter, int widthFrameHD, int heightFrameHD);
void update_tracker(Tracker* tracker, float* locations, float* classes, float* scores, 
                   int num_detections, float threshold, char** labels);
void free_tracker(Tracker* tracker);