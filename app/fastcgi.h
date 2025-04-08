#pragma once

#include "fcgi_stdio.h"
#include <jansson.h>
#include <pthread.h>

#include "detection.h"
#include "roi.h"
#include "counting.h"

// Define the number of vehicle types for PCU
#define NUM_VEHICLE_TYPES 7

// Thread for handling FastCGI requests
extern pthread_t fcgi_thread;
extern volatile int fcgi_running;

extern double confidence;
extern double pixels_per_meter;
extern bool first_wrongway;
extern bool second_wrongway;
extern bool first_truckright;
extern bool second_truckright;
extern int first_overspeed;
extern int second_overspeed;

extern float pcu_values[NUM_VEHICLE_TYPES];

// Structure to handle multiple points for crosslines
typedef struct
{
    int num_points;
    LinePoint points[MAX_SEGMENTS];
} MultiLineCoordinates;

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
} LineCoordinates;

// Structure to store incident data
typedef struct {
    int timer;
    bool accident;
    bool broken;
    bool stop;
    bool block;
    bool construction;
} IncidentData;

// Global variables to store incident states
extern IncidentData first_incidents;
extern IncidentData second_incidents;

// Structure to store limit speed data
typedef struct {
    int min;
    int max;
} LimitSpeedData;

// Global variables to store limit speed states
extern LimitSpeedData first_limitspeed;
extern LimitSpeedData second_limitspeed;

// Global variables to track if incidents have been received
extern bool first_incidents_received;
extern bool second_incidents_received;

extern bool first_overspeed_received;
extern bool second_overspeed_received;

extern bool first_limitspeed_received;
extern bool second_limitspeed_received;

// Function declarations
void process_polygon(Polygon** roi, json_t *json_data);
MultiLineCoordinates process_crossline(json_t *json_data);
LineCoordinates extract_two_point_coords(MultiLineCoordinates multi_coords);
void set_crossline_values(const char *name_param, json_t *json_data);
double process_slider(json_t *json_data);
bool process_toggle(json_t *json_data);
IncidentData process_incidents(json_t *json_data, bool *received_flag);
int process_overspeed(json_t *json_data, bool *received_flag);
LimitSpeedData process_limitspeed(json_t *json_data, bool *received_flag);
void process_pcu(json_t *json_data, float *pcu_values);
void set_name_values(const char *name_param, json_t *json_data);
int ensure_storage_directory(void);
char *create_filename(const char *name_param);
int save_to_file(const char *name_param, json_t *data);
json_t *load_from_file(const char *name_param);
char *get_file_contents(const char *path);
void get_parameters(void);
void send_json_response(FCGX_Stream *out, json_t *json);
json_t *parse_query_params(const char *query);
const char *get_name_param(json_t *query_params);
json_t *get_all_files_data(void);
void handle_get_request(FCGX_Stream *out, json_t *query_params);
void handle_post_request(FCGX_Request *request, const char *content_type, json_t *query_params);
void handle_put_request(FCGX_Request *request, const char *content_type, json_t *query_params);
void handle_delete_request(FCGX_Stream *out, json_t *query_params);
void cleanup_fcgi_resources(FCGX_Request *request, int sock);
void* fcgi_thread_func(void* arg);