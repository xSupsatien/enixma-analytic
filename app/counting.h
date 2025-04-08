#pragma once

#include "deepsort.h"
#include <glib.h>
#include <stdbool.h>
#include <jansson.h>  // Add this include for JSON support

#define MAX_LANES 4
#define MAX_SEGMENTS 5  // Maximum number of points to define lane segments (4 lanes = 5 points)

#define DAILY_ARRAY_SIZE 24
#define WEEKLY_ARRAY_SIZE 7
#define HOURLY_VELOCITY_BUFFER_SIZE 10000  // Buffer to store velocity data for the last hour

// Structures for the counting system
typedef struct {
    float x, y;  // Normalized coordinates (0-1)
} LinePoint;

// Structure to store velocity data for each counted object
typedef struct {
    float velocity;   // Speed in km/h
    gint64 timestamp; // When the object was counted
    int class_id;     // Vehicle class
} VelocityRecord;

typedef struct {
    LinePoint points[MAX_SEGMENTS];
    int num_points;
    int num_lanes;
    int* up_counts;       // Per-line counters
    int* down_counts;     // Per-line counters
    gint64* timestamps;   // Per-line timestamps
} MultiLaneLine;

typedef struct {
    MultiLaneLine line1;
    MultiLaneLine line2;
    bool line1_direction;
    bool line2_direction;
    bool use_second_line;
    int num_classes;
    
    // New fields for velocity tracking
    VelocityRecord velocity_buffer[HOURLY_VELOCITY_BUFFER_SIZE];
    int velocity_buffer_count;
    int velocity_buffer_index;
} CountingSystem;

// Global variable declaration
extern CountingSystem* counting_system;

extern int daily_vehicle_count[DAILY_ARRAY_SIZE];
extern int weekly_vehicle_count[WEEKLY_ARRAY_SIZE];

extern double daily_vehicle_pcu[DAILY_ARRAY_SIZE];
extern double weekly_vehicle_pcu[WEEKLY_ARRAY_SIZE];

extern double daily_average_speed[DAILY_ARRAY_SIZE];
extern double weekly_average_speed[WEEKLY_ARRAY_SIZE];

// Core functionality
CountingSystem* init_counting_system(int num_classes, int num_lanes, LinePoint* points);
void free_counting_system(CountingSystem* system);

// Define line identifiers for better readability
typedef enum {
    LINE_1 = 0,
    LINE_2 = 1
} LineId;

// Line and counting management
void update_line_points(CountingSystem* system, LineId line_id, LinePoint* points, int num_points);
bool resize_line_lanes(CountingSystem* system, LineId line_id, int new_lane_count);
void update_counting(CountingSystem* system, TrackedObject* obj);

// Data retrieval
void get_lane_counts(CountingSystem* system, LineId line_id, int class_id, int lane_id, 
                     int* up_count, int* down_count);

// Internal helper functions
bool is_segment_crossed(Point* p1, Point* p2, LinePoint* seg_start, LinePoint* seg_end, int* lane_id);
int get_crossing_direction(Point* p1, Point* p2, LinePoint* seg_start, LinePoint* seg_end);

// Midnight reset functionality
void reset_all_counters(CountingSystem* system);
bool check_midnight_reset(CountingSystem *system);

void add_velocity_record(CountingSystem* system, float velocity, int class_id);
float get_average_velocity(CountingSystem* system, int time_window_ms, int class_id);
void clean_velocity_buffer(CountingSystem* system, gint64 max_age_us);

// Backup and restore functionality
json_t* counting_data_to_json(CountingSystem* system);
bool save_counting_data(CountingSystem* system, const char* filename);
bool load_counting_data(CountingSystem* system, const char* filename);
bool save_vehicle_count_data(CountingSystem* system, const char* filename);
bool save_vehicle_pcu_data(CountingSystem* system, const char* filename);

bool save_chart_data(const char* filename, int* chart_data, int array_size);
bool save_chart_data_double(const char* filename, double* chart_data, int array_size);
bool load_chart_data(const char* filename, int* chart_data, int array_size);
bool load_chart_data_double(const char* filename, double* chart_data, int array_size);

int calculate_total_count(CountingSystem* system);
float calculate_total_pcu(CountingSystem* system);
bool save_daily_vehicle_count_data(CountingSystem* system, const char* filename);
bool save_weekly_vehicle_count_data(CountingSystem* system, const char* filename);

bool save_daily_vehicle_pcu_data(CountingSystem* system, const char* filename);
bool save_weekly_vehicle_pcu_data(CountingSystem* system, const char* filename);

bool save_average_speed_data(CountingSystem* system, const char* filename);
bool save_daily_average_speed_data(CountingSystem* system, const char* filename);
bool save_weekly_average_speed_data(CountingSystem* system, const char* filename);

// Periodic backup functionality
bool check_periodic_backup(CountingSystem* system);

// Function to shift all elements in an integer array one position left
void shift_array_left(int array[], int array_size);
void shift_array_left_double(double array[], int array_size);