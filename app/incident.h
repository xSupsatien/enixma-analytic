#pragma once

#include <stdbool.h>
#include <time.h>
#include "deepsort.h"

// Event type definitions
typedef enum {
    EVENT_NONE = 0,
    EVENT_CAR_ACCIDENT = 1,
    EVENT_CAR_BROKEN = 2,
    EVENT_CAR_STOPPED = 3,
    EVENT_ROAD_BLOCKED = 4,
    EVENT_ROAD_CONSTRUCTION = 5
} EventType;

// Event state structure
typedef struct {
    EventType type;
    int object_id;
    time_t detection_time;
    bool reported;
    char description[256];
    int roi_index;  // Added to track which ROI the event belongs to (1 or 2)
} Event;

// Constants
#define MAX_EVENTS 100
#define PROXIMITY_THRESHOLD 0.3  // Objects are close if within 30% of frame width/height
#define MIN_EVENT_DISTANCE 0.2   // Minimum distance between events (normalized 0-1 coordinates)
#define MIN_EVENT_TIME_GAP 300   // Minimum time (in seconds) between similar events (5 minutes)

// ROI event settings structure
typedef struct {
    bool enabled;          // Master enable/disable for this ROI
    int timer;             // Timer value in seconds
    bool accident;         // Enable accident detection
    bool broken;           // Enable broken vehicle detection
    bool stop;             // Enable stopped vehicle detection
    bool block;            // Enable road block detection
    bool construction;     // Enable construction detection
} ROIEventSettings;

// Structure to store event locations (for spatial-temporal filtering)
typedef struct {
    int object_id;
    float center_x;
    float center_y;
    time_t timestamp;
    EventType type;
    int roi_index;
} EventLocation;

// Function declarations
void init_incident(void);
void process_events(Tracker* tracker);
float calculate_object_distance(TrackedObject* obj1, TrackedObject* obj2);
bool is_person_nearby(Tracker* tracker, TrackedObject* obj);
bool is_car_accident_event(Tracker* tracker, TrackedObject* obj);
void add_event(EventType type, int object_id, const char* description, int roi_index, Tracker* tracker);
bool is_vehicle(int class_id);

// Event termination functions
void check_event_termination(Tracker* tracker);
void reset_events_for_object(int object_id);

// Timer-related function declarations
void init_object_timer(TrackedObject* obj);
void reset_object_timer(TrackedObject* obj);

// ROI event settings
void update_roi_event_settings(int roi_index, ROIEventSettings settings);
bool is_object_in_roi(TrackedObject* obj, int roi_index);
bool is_event_enabled(EventType type, int roi_index);

// Spatial-temporal filtering functions
bool is_too_close_to_existing_events(EventType type, float cx, float cy, int roi_index);
void store_event_location(int object_id, TrackedObject* obj, EventType type, int roi_index);

// Set the class IDs based on your model's class mapping
#define BIKE_CLASS_ID 1
#define PERSON_CLASS_ID 7
#define CONE_CLASS_ID 8

// Global event system state
extern Event event_list[MAX_EVENTS];
extern int event_count;

// Global ROI event settings
extern ROIEventSettings roi1_event_settings;
extern ROIEventSettings roi2_event_settings;

extern char *incident_types[];