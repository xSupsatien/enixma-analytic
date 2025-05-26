#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <float.h> // For FLT_MAX

#include "incident.h"
#include "deepsort.h"
#include "roi.h"
#include "detection.h"
#include "imwrite.h"
#include "event.h"

// Global event tracking
Event event_list[MAX_EVENTS];
int event_count = 0;

// Global ROI event settings
ROIEventSettings roi1_event_settings = {false, 30, false, false, false, false, false};
ROIEventSettings roi2_event_settings = {false, 30, false, false, false, false, false};

// Store start times for object IDs to ensure consistency
#define MAX_TRACKED_IDS 10000
static time_t object_start_times[MAX_TRACKED_IDS] = {0};
static bool initialized = false;

// Array to store locations of events
#define MAX_EVENT_LOCATIONS 1000
static EventLocation event_locations[MAX_EVENT_LOCATIONS];
static int event_location_count = 0;

char *incident_types[] = {
    "unknown",
    "car accident",
    "car breakdown",
    "car stop",
    "road construction",
    "road block",
    "wrong way",
    "truck right",
    "over speed",
    "limit speed"};

// Function to store event location
void store_event_location(int object_id, TrackedObject *obj, EventType type, int roi_index)
{
    if (event_location_count >= MAX_EVENT_LOCATIONS)
    {
        // Remove oldest location if array is full
        memmove(&event_locations[0], &event_locations[1],
                sizeof(EventLocation) * (MAX_EVENT_LOCATIONS - 1));
        event_location_count--;
    }

    // Calculate object center
    float cx = (obj->bbox[1] + obj->bbox[3]) / 2.0f;
    float cy = (obj->bbox[0] + obj->bbox[2]) / 2.0f;

    // Store the location
    EventLocation *loc = &event_locations[event_location_count++];
    loc->object_id = object_id;
    loc->center_x = cx;
    loc->center_y = cy;
    loc->timestamp = time(NULL);
    loc->type = type;
    loc->roi_index = roi_index;

    // syslog(LOG_INFO, "Stored event location for object ID %d at (%.3f, %.3f), type %d, ROI %d",
    //        object_id, cx, cy, type, roi_index);
}

// Check if a new event is too close to existing events
bool is_too_close_to_existing_events(EventType type, float cx, float cy, int roi_index)
{
    time_t current_time = time(NULL);

    for (int i = 0; i < event_location_count; i++)
    {
        EventLocation *existing = &event_locations[i];

        // Skip if in different ROI
        if (existing->roi_index != roi_index)
        {
            continue;
        }

        // Skip if different event type
        if (existing->type != type)
        {
            continue;
        }

        // Check time difference - only filter if less than MIN_EVENT_TIME_GAP seconds have passed
        double time_diff = difftime(current_time, existing->timestamp);
        if (time_diff > MIN_EVENT_TIME_GAP)
        {
            continue; // Event is old enough, don't filter
        }

        // Check spatial proximity
        float dist = sqrt(
            (cx - existing->center_x) * (cx - existing->center_x) +
            (cy - existing->center_y) * (cy - existing->center_y));

        if (dist < MIN_EVENT_DISTANCE)
        {
            // syslog(LOG_INFO, "Filtered duplicate event: too close to existing event ID %d (dist: %.3f, time: %.1fs)",
            //        existing->object_id, dist, time_diff);
            return true; // Too close to an existing event
        }
    }

    return false; // Not too close to any existing event
}

// Initialize the event detection system
void init_incident()
{
    event_count = 0;
    memset(event_list, 0, sizeof(event_list));

    // Initialize event locations
    event_location_count = 0;
    memset(event_locations, 0, sizeof(event_locations));

    // Initialize the start time tracking array
    memset(object_start_times, 0, sizeof(object_start_times));
    initialized = true;

    // Initialize ROI event settings
    roi1_event_settings.enabled = false;
    roi1_event_settings.timer = 30; // Default timer value in seconds
    roi1_event_settings.accident = false;
    roi1_event_settings.broken = false;
    roi1_event_settings.stop = false;
    roi1_event_settings.block = false;
    roi1_event_settings.construction = false;

    roi2_event_settings.enabled = false;
    roi2_event_settings.timer = 30; // Default timer value in seconds
    roi2_event_settings.accident = false;
    roi2_event_settings.broken = false;
    roi2_event_settings.stop = false;
    roi2_event_settings.block = false;
    roi2_event_settings.construction = false;

    // syslog(LOG_INFO, "Event detection system initialized");
}

// Update ROI event settings
void update_roi_event_settings(int roi_index, ROIEventSettings settings)
{
    if (roi_index == 1)
    {
        roi1_event_settings = settings;
        // syslog(LOG_INFO, "Updated ROI1 event settings - enabled: %d, timer: %d, accident: %d, broken: %d, stop: %d, block: %d, construction: %d",
        //        roi1_event_settings.enabled,
        //        roi1_event_settings.timer,
        //        roi1_event_settings.accident,
        //        roi1_event_settings.broken,
        //        roi1_event_settings.stop,
        //        roi1_event_settings.block,
        //        roi1_event_settings.construction);
    }
    else if (roi_index == 2)
    {
        roi2_event_settings = settings;
        // syslog(LOG_INFO, "Updated ROI2 event settings - enabled: %d, timer: %d, accident: %d, broken: %d, stop: %d, block: %d, construction: %d",
        //        roi2_event_settings.enabled,
        //        roi2_event_settings.timer,
        //        roi2_event_settings.accident,
        //        roi2_event_settings.broken,
        //        roi2_event_settings.stop,
        //        roi2_event_settings.block,
        //        roi2_event_settings.construction);
    }
}

// Check if event type is enabled for the given ROI
bool is_event_enabled(EventType type, int roi_index)
{
    if (roi_index != 1 && roi_index != 2)
    {
        return false;
    }

    ROIEventSettings *settings = (roi_index == 1) ? &roi1_event_settings : &roi2_event_settings;

    // First check if events are enabled at all for this ROI
    if (!settings->enabled)
    {
        return false;
    }

    // Then check if the specific event type is enabled
    switch (type)
    {
    case EVENT_CAR_STOPPED:
        return settings->stop;
    case EVENT_CAR_BROKEN:
        return settings->broken;
    case EVENT_CAR_ACCIDENT:
        return settings->accident;
    case EVENT_ROAD_BLOCKED:
        return settings->block;
    case EVENT_ROAD_CONSTRUCTION:
        return settings->construction;
    default:
        return false;
    }
}

// Check if object is in a specific ROI
bool is_object_in_roi(TrackedObject *obj, int roi_index)
{
    if (!obj)
    {
        return false;
    }

    // Calculate object center point
    float cx = (obj->bbox[1] + obj->bbox[3]) / 2.0f;
    float cy = (obj->bbox[0] + obj->bbox[2]) / 2.0f;

    // Check against ROI polygons
    if (roi_index == 1 && roi1)
    {
        return is_point_in_polygon(cx, cy, roi1);
    }
    else if (roi_index == 2 && roi2)
    {
        return is_point_in_polygon(cx, cy, roi2);
    }

    return false;
}

// Initialize object timer with event detection fields
void init_object_timer(TrackedObject *obj)
{
    // Initialize the array on first use if not already done
    if (!initialized)
    {
        memset(object_start_times, 0, sizeof(object_start_times));
        initialized = true;
        // syslog(LOG_INFO, "Initialized object start time tracking array");
    }

    int id = obj->track_id % MAX_TRACKED_IDS;

    // Only set start time if it hasn't been set before
    if (object_start_times[id] == 0)
    {
        obj->start_time = time(NULL);
        object_start_times[id] = obj->start_time;
    }
    else
    {
        // Use the existing start time
        obj->start_time = object_start_times[id];
    }

    obj->timer_active = true;

    // Initialize event detection fields
    obj->event_check_initialized = false;
    obj->event_check_start = 0;
    obj->event_detected = false;
}

// Reset object timer
void reset_object_timer(TrackedObject *obj)
{
    int id = obj->track_id % MAX_TRACKED_IDS;

    // If the object previously had an event detected, reset any associated events
    if (obj->event_detected)
    {
        reset_events_for_object(obj->track_id);
    }

    obj->start_time = time(NULL);
    object_start_times[id] = obj->start_time;

    // Reset event detection fields
    obj->event_check_initialized = false;
    obj->event_check_start = 0;
    obj->event_detected = false;
}

// Function to check if object is a vehicle type
bool is_vehicle(int class_id)
{
    // Check if the class ID matches any of the vehicle types
    return (class_id == 0 || // car
            class_id == 1 || // bike
            class_id == 2 || // truck
            class_id == 3 || // bus
            class_id == 4 || // taxi
            class_id == 5 || // pickup
            class_id == 6);  // trailer
                             // Not vehicles: 7 (person), 8 (cone)
}

// Function to calculate distance between objects (normalized 0-1 coordinates)
float calculate_object_distance(TrackedObject *obj1, TrackedObject *obj2)
{
    // Calculate centers
    float cx1 = (obj1->bbox[1] + obj1->bbox[3]) / 2.0f;
    float cy1 = (obj1->bbox[0] + obj1->bbox[2]) / 2.0f;

    float cx2 = (obj2->bbox[1] + obj2->bbox[3]) / 2.0f;
    float cy2 = (obj2->bbox[0] + obj2->bbox[2]) / 2.0f;

    // Calculate Euclidean distance
    float dx = cx1 - cx2;
    float dy = cy1 - cy2;

    return sqrt(dx * dx + dy * dy);
}

// Function to check for presence of person near an object
bool is_person_nearby(Tracker *tracker, TrackedObject *obj)
{
    for (int i = 0; i < tracker->count; i++)
    {
        TrackedObject *other = &tracker->objects[i];

        // Skip if same object or not a person
        if (other->track_id == obj->track_id || other->class_id != PERSON_CLASS_ID)
        {
            continue;
        }

        // Check if person is nearby
        if (calculate_object_distance(obj, other) < PROXIMITY_THRESHOLD)
        {
            // syslog(LOG_INFO, "Person detected near object ID %d", obj->track_id);
            return true;
        }
    }
    return false;
}

// Function to check for car accident events
bool is_car_accident_event(Tracker *tracker, TrackedObject *obj)
{
    // Skip if object is a cone
    if (obj->class_id == CONE_CLASS_ID)
    {
        return false;
    }

    bool person_nearby = false;
    bool other_vehicle_nearby = false;
    int nearby_vehicle_id = -1;
    (void)nearby_vehicle_id;

    // Check for nearby objects
    for (int i = 0; i < tracker->count; i++)
    {
        TrackedObject *other = &tracker->objects[i];

        // Skip if same object
        if (other->track_id == obj->track_id)
        {
            continue;
        }

        // Calculate distance between objects
        float distance = calculate_object_distance(obj, other);

        if (distance < PROXIMITY_THRESHOLD)
        {
            // Check if it's a person
            if (other->class_id == PERSON_CLASS_ID)
            {
                person_nearby = true;
            }
            // Check if it's a vehicle
            else if (is_vehicle(other->class_id))
            {
                // Check if this object has also been stationary for 30+ seconds
                time_t current_time = time(NULL);
                // Use double for time difference
                double other_elapsed = difftime(current_time, other->start_time);

                // syslog(LOG_INFO, "Nearby vehicle ID %d has been stationary for %.1f seconds",
                //        other->track_id, other_elapsed);

                // Check against the appropriate timer threshold based on ROI
                int timer_threshold = 30; // Default

                // Determine which ROI the object is in
                if (is_object_in_roi(obj, 1) && roi1_event_settings.enabled)
                {
                    timer_threshold = roi1_event_settings.timer;
                }
                else if (is_object_in_roi(obj, 2) && roi2_event_settings.enabled)
                {
                    timer_threshold = roi2_event_settings.timer;
                }

                if (other_elapsed >= timer_threshold)
                {
                    other_vehicle_nearby = true;
                    nearby_vehicle_id = other->track_id;
                }
            }
        }
    }

    // Debug logging
    // if (person_nearby && other_vehicle_nearby)
    // {
    //     syslog(LOG_INFO, "Potential accident: Object ID %d near stationary vehicle ID %d with person nearby",
    //            obj->track_id, nearby_vehicle_id);
    // }

    // Return true if both a person and another stationary vehicle are nearby
    return person_nearby && other_vehicle_nearby;
}

// Add a new event to the event list with ROI information
void add_event(EventType type, int object_id, const char *description, int roi_index, Tracker *tracker)
{
    // Find the object in the tracker to get its location
    TrackedObject *obj = NULL;
    for (int i = 0; i < tracker->count; i++)
    {
        if (tracker->objects[i].track_id == object_id)
        {
            obj = &tracker->objects[i];
            break;
        }
    }

    if (obj == NULL)
    {
        syslog(LOG_WARNING, "Could not find object ID %d for spatial filtering", object_id);
        return; // Skip event creation if object isn't found
    }

    // Calculate object center
    float cx = (obj->bbox[1] + obj->bbox[3]) / 2.0f;
    float cy = (obj->bbox[0] + obj->bbox[2]) / 2.0f;

    // Check if too close to existing events
    if (is_too_close_to_existing_events(type, cx, cy, roi_index))
    {
        // syslog(LOG_INFO, "Skipping event creation: too close to existing event");
        return;
    }

    // If we've reached this point, we can create the event
    if (event_count >= MAX_EVENTS)
    {
        // Remove oldest event if list is full
        memmove(&event_list[0], &event_list[1], sizeof(Event) * (MAX_EVENTS - 1));
        event_count--;
    }

    Event *event = &event_list[event_count++];
    event->type = type;
    event->object_id = object_id;
    event->detection_time = time(NULL);
    event->reported = false;
    event->roi_index = roi_index;
    strncpy(event->description, description, sizeof(event->description) - 1);
    event->description[sizeof(event->description) - 1] = '\0';

    // Store the event location for future reference
    store_event_location(object_id, obj, type, roi_index);

    // Log the event
    // syslog(LOG_INFO, "EVENT DETECTED: %s (ID: %d, Type: %d, ROI: %d)",
    //        description, object_id, type, roi_index);

    if (type > 0)
    {
        char filename[64]; // Pre-allocated buffer with sufficient size
        time_t timestamp = time(NULL);

        int written = snprintf(filename, sizeof(filename), "%ld-%i", (long)timestamp, type);

        if (written < 0 || (size_t)written >= sizeof(filename))
        {
            syslog(LOG_ERR, "Failed to create filename (buffer too small or format error)");
        }
        else
        {
            // syslog(LOG_INFO, "Writing image to filename: %s", filename);

            imwrite(filename, context.addresses.ppOutputAddrHD);
            send_event_incidents(app_data_incidents, context.label.labels[obj->class_id], incident_types[type], roi_index, 0, filename);
        }

        obj->event_detected = true;
    }
}

// Function to check if an event is over
void check_event_termination(Tracker *tracker)
{
    if (!tracker)
    {
        syslog(LOG_ERR, "Tracker is NULL in check_event_termination");
        return;
    }

    // Check existing events
    for (int i = 0; i < event_count; i++)
    {
        Event *event = &event_list[i];

        // Skip events that have been marked as terminated
        if (event->reported)
        {
            continue;
        }

        // Check if the object associated with this event is still being tracked
        bool object_exists = false;
        bool object_moving = false;

        for (int j = 0; j < tracker->count; j++)
        {
            TrackedObject *obj = &tracker->objects[j];

            // Found the object with this ID
            if (obj->track_id == event->object_id)
            {
                object_exists = true;

                // Check if the object is now moving
                time_t current_time = time(NULL);
                double elapsed_seconds = difftime(current_time, obj->start_time);

                // Determine which timer to use based on ROI
                int timer_threshold = 30; // Default
                if (event->roi_index == 1 && roi1_event_settings.enabled)
                {
                    timer_threshold = roi1_event_settings.timer;
                }
                else if (event->roi_index == 2 && roi2_event_settings.enabled)
                {
                    timer_threshold = roi2_event_settings.timer;
                }

                // If timer has been reset or less than the threshold have passed, the object is moving
                if (elapsed_seconds < timer_threshold)
                {
                    object_moving = true;
                }

                // No need to check further objects
                break;
            }
        }

        // Event is over if the object is no longer tracked or has started moving
        if (!object_exists || object_moving)
        {
            // Log that the event is over
            // syslog(LOG_INFO, "EVENT OVER: %s (ID: %d, Type: %d, ROI: %d, Duration: %.1f minutes)",
            //        event->description,
            //        event->object_id,
            //        event->type,
            //        event->roi_index,
            //        difftime(time(NULL), event->detection_time) / 60.0);

            // Mark as reported so we don't report it again
            event->reported = true;
        }
    }
}

// Function to reset all events for a newly moving object
void reset_events_for_object(int object_id)
{
    for (int i = 0; i < event_count; i++)
    {
        if (event_list[i].object_id == object_id && !event_list[i].reported)
        {
            // Log that the event is over
            // syslog(LOG_INFO, "EVENT OVER: %s (ID: %d, Type: %d, ROI: %d, Duration: %.1f minutes)",
            //        event_list[i].description,
            //        event_list[i].object_id,
            //        event_list[i].type,
            //        event_list[i].roi_index,
            //        difftime(time(NULL), event_list[i].detection_time) / 60.0);

            // Mark as reported
            event_list[i].reported = true;
        }
    }
}

// Process events for all tracked objects
void process_events(Tracker *tracker)
{
    if (!tracker)
    {
        syslog(LOG_ERR, "Tracker is NULL in process_events");
        return;
    }

    // Skip if no ROI event detection is enabled
    if (!roi1_event_settings.enabled && !roi2_event_settings.enabled)
    {
        return;
    }

    // First, check if any existing events have terminated
    check_event_termination(tracker);

    // Process all tracked objects
    for (int i = 0; i < tracker->count; i++)
    {
        TrackedObject *obj = &tracker->objects[i];

        // Skip if object is a person or doesn't meet minimum hits
        if (obj->class_id == PERSON_CLASS_ID || obj->hits < tracker->min_hits)
        {
            continue;
        }

        // Skip if event already detected
        if (obj->event_detected)
        {
            continue;
        }

        // Determine which ROI the object is in (if any)
        int roi_index = 0;
        if (is_object_in_roi(obj, 1) && roi1_event_settings.enabled)
        {
            roi_index = 1;
        }
        else if (is_object_in_roi(obj, 2) && roi2_event_settings.enabled)
        {
            roi_index = 2;
        }

        // Skip if object is not in any enabled ROI
        if (roi_index == 0)
        {
            continue;
        }

        // Get the appropriate timer threshold for this ROI
        int timer_threshold = (roi_index == 1) ? roi1_event_settings.timer : roi2_event_settings.timer;

        time_t current_time = time(NULL);
        // Use double for time difference
        double elapsed_seconds = difftime(current_time, obj->start_time);

        if (elapsed_seconds >= timer_threshold)
        {
            // Object has been stationary for at least the timer threshold
            if (!obj->event_check_initialized)
            {
                // Start the 15-second event check period
                obj->event_check_start = current_time;
                obj->event_check_initialized = true;
                continue;
            }

            // Check if 15 seconds have passed since starting event check
            double check_elapsed = difftime(current_time, obj->event_check_start);

            if (check_elapsed >= 15.0)
            {
                bool person_nearby = is_person_nearby(tracker, obj);

                // Check for car accident event first
                if (is_event_enabled(EVENT_CAR_ACCIDENT, roi_index) && is_car_accident_event(tracker, obj))
                {
                    // Event 1: Car accident
                    add_event(EVENT_CAR_ACCIDENT, obj->track_id,
                              "Car accident detected - Multiple stationary vehicles and persons present",
                              roi_index, tracker);
                }
                else if (person_nearby)
                {
                    // Determine if it's Event 2 or Event 5
                    if (is_vehicle(obj->class_id) && is_event_enabled(EVENT_CAR_BROKEN, roi_index))
                    {
                        // Event 2: Car broken
                        add_event(EVENT_CAR_BROKEN, obj->track_id,
                                  "Vehicle broken down - Vehicle stopped with person nearby",
                                  roi_index, tracker);
                    }
                    else if (obj->class_id == CONE_CLASS_ID && is_event_enabled(EVENT_ROAD_CONSTRUCTION, roi_index))
                    {
                        // Event 5: Road construction
                        add_event(EVENT_ROAD_CONSTRUCTION, obj->track_id,
                                  "Road construction - Object stationary with person nearby",
                                  roi_index, tracker);
                    }
                }
                else
                {
                    // Determine if it's Event 3 or Event 4
                    if (is_vehicle(obj->class_id) && is_event_enabled(EVENT_CAR_STOPPED, roi_index))
                    {
                        // Event 3: Car stopped
                        add_event(EVENT_CAR_STOPPED, obj->track_id,
                                  "Vehicle stopped - Vehicle stationary with no person nearby",
                                  roi_index, tracker);
                    }
                    else if (obj->class_id == CONE_CLASS_ID && is_event_enabled(EVENT_ROAD_BLOCKED, roi_index))
                    {
                        // Event 4: Road blocked
                        add_event(EVENT_ROAD_BLOCKED, obj->track_id,
                                  "Road blocked - Object stationary with no person nearby",
                                  roi_index, tracker);
                    }
                }
            }
        }
    }
}