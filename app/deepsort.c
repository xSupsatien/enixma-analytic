#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "deepsort.h"
#include "incident.h"
#include "detection.h"
#include "counting.h"

Tracker *tracker = NULL;

// Initialize tracker
float frame_time = 1.0f / 30.0f; // 30 FPS

// Calculate velocity using last two trajectory points
void update_velocity(TrackedObject *obj, float frame_time, float pixels_per_meter, int widthFrameHD, int heightFrameHD)
{
    // Need at least 2 points to calculate velocity
    if (obj->trajectory_count < 2)
    {
        obj->velocity[0] = 0;
        obj->velocity[1] = 0;
        obj->speed_kmh = 0;
        return;
    }

    // Get the last two points from trajectory
    Point *current = &obj->trajectory[obj->trajectory_count - 1];
    Point *previous = &obj->trajectory[obj->trajectory_count - 2];

    // Calculate velocity components
    float dx = (current->x - previous->x);
    float dy = (current->y - previous->y);

    obj->velocity[0] = dx;
    obj->velocity[1] = dy;
    obj->speed_kmh = calculate_speed_kmh(dx, dy, frame_time, pixels_per_meter, widthFrameHD, heightFrameHD);
}

// Initialize tracker
Tracker *init_tracker(int capacity, float iou_threshold, int max_age, int min_hits)
{
    Tracker *tracker = (Tracker *)malloc(sizeof(Tracker));
    if (!tracker)
        return NULL;

    tracker->objects = (TrackedObject *)malloc(sizeof(TrackedObject) * capacity);
    if (!tracker->objects)
    {
        free(tracker);
        return NULL;
    }

    tracker->count = 0;
    tracker->capacity = capacity;
    tracker->iou_threshold = iou_threshold;
    tracker->max_age = max_age;
    tracker->min_hits = min_hits;
    tracker->next_track_id = 0;
    return tracker;
}

// Calculate IoU (Intersection over Union)
float calculate_iou(float *box1, float *box2)
{
    float x1 = fmax(box1[1], box2[1]);
    float y1 = fmax(box1[0], box2[0]);
    float x2 = fmin(box1[3], box2[3]);
    float y2 = fmin(box1[2], box2[2]);

    float intersection = fmax(0, x2 - x1) * fmax(0, y2 - y1);
    float box1_area = (box1[3] - box1[1]) * (box1[2] - box1[0]);
    float box2_area = (box2[3] - box2[1]) * (box2[2] - box2[0]);

    return intersection / (box1_area + box2_area - intersection);
}

// Calculate speed in km/hr
float calculate_speed_kmh(float dx, float dy, float frame_time, float pixels_per_meter, int widthFrameHD, int heightFrameHD)
{
    dx = dx * widthFrameHD;
    dy = dy * heightFrameHD;

    float displacement = sqrt(dx * dx + dy * dy);
    float meters = displacement / pixels_per_meter;
    float speed_ms = meters / frame_time;
    return speed_ms * 3.6; // Convert m/s to km/h
}

// Update tracker with new detections
void update_tracker(Tracker *tracker,
                    float *locations,
                    float *classes,
                    float *scores,
                    int num_detections,
                    float threshold,
                    char **labels)
{
    (void)labels;

    if (!tracker)
        return;

    // Update age of all existing tracks
    for (int i = 0; i < tracker->count; i++)
    {
        update_counting(counting_system, &tracker->objects[i]);
        tracker->objects[i].time_since_update++;
    }

    // Process new detections
    for (int i = 0; i < num_detections; i++)
    {
        if (scores[i] < threshold / 100.0)
            continue;

        float *curr_bbox = &locations[4 * i];
        bool matched = false;

        // Skip if detection is outside ROI (when ROI is defined)
        if ((roi1 && !is_in_roi(curr_bbox, roi1)) && (roi2 && !is_in_roi(curr_bbox, roi2)))
        {
            continue;
        }

        // Try to match with existing tracks
        for (int j = 0; j < tracker->count; j++)
        {
            if (tracker->objects[j].time_since_update > tracker->max_age)
                continue;

            float iou = calculate_iou(curr_bbox, tracker->objects[j].bbox);
            float iou_threshold = ((int)classes[i] == 1) ? 0.1f : tracker->iou_threshold;

            if (iou >= iou_threshold)
            {
                // Store original event-related states
                bool was_event_detected = tracker->objects[j].event_detected;
                bool was_event_initialized = tracker->objects[j].event_check_initialized;
                time_t event_check_start = tracker->objects[j].event_check_start;
                time_t start_time = tracker->objects[j].start_time;

                // Update existing track
                memcpy(tracker->objects[j].bbox, curr_bbox, 4 * sizeof(float));
                tracker->objects[j].score = scores[i];
                tracker->objects[j].class_id = (int)classes[i];
                tracker->objects[j].hits++;
                tracker->objects[j].time_since_update = 0;

                // Preserve event detection state
                tracker->objects[j].event_detected = was_event_detected;
                tracker->objects[j].event_check_initialized = was_event_initialized;
                tracker->objects[j].event_check_start = event_check_start;
                tracker->objects[j].start_time = start_time;

                // Calculate center point for trajectory
                float cx = (curr_bbox[1] + curr_bbox[3]) / 2.0f;
                float cy = (curr_bbox[0] + curr_bbox[2]) / 2.0f;

                // Only add point if it's different from the last trajectory point
                if (tracker->objects[j].trajectory_count == 0)
                {
                    // First point, always add it
                    tracker->objects[j].trajectory[0].x = cx;
                    tracker->objects[j].trajectory[0].y = cy;
                    tracker->objects[j].trajectory_count = 1;
                }
                else
                {
                    // Get last trajectory point
                    Point *last_point = &tracker->objects[j].trajectory[tracker->objects[j].trajectory_count - 1];

                    // Check if position has changed
                    if (fabs(last_point->x - cx) > EPSILON || fabs(last_point->y - cy) > EPSILON)
                    {
                        if (tracker->objects[j].trajectory_count < MAX_TRAJECTORY_POINTS)
                        {
                            int idx = tracker->objects[j].trajectory_count++;
                            tracker->objects[j].trajectory[idx].x = cx;
                            tracker->objects[j].trajectory[idx].y = cy;
                        }
                        else
                        {
                            for (int k = 0; k < MAX_TRAJECTORY_POINTS - 1; k++)
                            {
                                tracker->objects[j].trajectory[k] = tracker->objects[j].trajectory[k + 1];
                            }
                            tracker->objects[j].trajectory[MAX_TRAJECTORY_POINTS - 1].x = cx;
                            tracker->objects[j].trajectory[MAX_TRAJECTORY_POINTS - 1].y = cy;
                        }

                        // // Update velocity after adding new trajectory point
                        // update_velocity(&tracker->objects[j], frame_time, pixels_per_meter, context.resolution.widthFrameHD, context.resolution.heightFrameHD);

                        // If object has moved significantly, reset the timer and event detection
                        float movement = sqrt(
                            tracker->objects[j].velocity[0] * tracker->objects[j].velocity[0] +
                            tracker->objects[j].velocity[1] * tracker->objects[j].velocity[1]);

                        if (movement > 0.01f)
                        { // Threshold for considering movement significant
                            reset_object_timer(&tracker->objects[j]);
                        }
                    }
                }

                matched = true;
                break;
            }
        }

        // If no match found and we have capacity, create new track
        if (!matched && tracker->count < tracker->capacity)
        {
            TrackedObject new_obj = {0};
            memcpy(new_obj.bbox, curr_bbox, 4 * sizeof(float));
            new_obj.score = scores[i];
            new_obj.class_id = (int)classes[i];

            // Reset track_id if it reaches maximum
            if (tracker->next_track_id >= MAX_TRACK_ID)
            {
                tracker->next_track_id = 0;
                // syslog(LOG_INFO, "Track ID counter reset to 0");
            }

            new_obj.track_id = tracker->next_track_id++;

            new_obj.hits = 1;
            new_obj.age = 1;
            new_obj.time_since_update = 0;
            new_obj.speed_kmh = 0;

            // Initialize first trajectory point
            float cx = (curr_bbox[1] + curr_bbox[3]) / 2.0f;
            float cy = (curr_bbox[0] + curr_bbox[2]) / 2.0f;
            new_obj.trajectory[0].x = cx;
            new_obj.trajectory[0].y = cy;
            new_obj.trajectory_count = 1;

            // Initialize timer for the new object
            init_object_timer(&new_obj);

            tracker->objects[tracker->count++] = new_obj;
        }
    }

    // Delete old tracks and compress the array
    int write_index = 0;
    for (int read_index = 0; read_index < tracker->count; read_index++)
    {
        if (tracker->objects[read_index].time_since_update <= tracker->max_age)
        {
            if (write_index != read_index)
            {
                // When moving objects during compression, make a complete copy
                TrackedObject temp = tracker->objects[read_index];
                tracker->objects[write_index] = temp;
            }
            write_index++;
        }
        // else
        // {
        //     syslog(LOG_INFO, "Deleting track %d due to age %d exceeding max_age %d",
        //            tracker->objects[read_index].track_id,
        //            tracker->objects[read_index].time_since_update,
        //            tracker->max_age);
        // }
    }
    tracker->count = write_index;
    
    // // Log active tracks
    // for (int i = 0; i < tracker->count; i++) {
    //     if (tracker->objects[i].hits < tracker->min_hits) continue;

    //     // Build trajectory string
    //     char trajectory_str[1024] = "";
    //     size_t offset = 0;
    //     for (int j = 0; j < tracker->objects[i].trajectory_count; j++) {
    //         int written = snprintf(trajectory_str + offset, sizeof(trajectory_str) - offset,
    //                              "[%.6f,%.6f] ",
    //                              tracker->objects[i].trajectory[j].x,
    //                              tracker->objects[i].trajectory[j].y);
    //         if (written < 0 || (size_t)written >= sizeof(trajectory_str) - offset) break;
    //         offset += written;
    //     }

    //     // Determine which ROI the object is in
    //     char roi_info[32] = "None";
    //     if (roi1 && is_in_roi(tracker->objects[i].bbox, roi1)) {
    //         if (roi2 && is_in_roi(tracker->objects[i].bbox, roi2)) {
    //             strcpy(roi_info, "ROI1 & ROI2");
    //         } else {
    //             strcpy(roi_info, "ROI1");
    //         }
    //     } else if (roi2 && is_in_roi(tracker->objects[i].bbox, roi2)) {
    //         strcpy(roi_info, "ROI2");
    //     }

    //     syslog(LOG_INFO,
    //            "Track %d: Class: %s - Score: %f - Locations: [%f,%f,%f,%f] - "
    //            "Velocity: [%f,%f] - Speed: %.2f km/h - Age: %d - Hits: %d - ROI: %s - Trajectory: %s",
    //            tracker->objects[i].track_id,
    //            labels[tracker->objects[i].class_id],
    //            tracker->objects[i].score,
    //            tracker->objects[i].bbox[0],
    //            tracker->objects[i].bbox[1],
    //            tracker->objects[i].bbox[2],
    //            tracker->objects[i].bbox[3],
    //            tracker->objects[i].velocity[0],
    //            tracker->objects[i].velocity[1],
    //            tracker->objects[i].speed_kmh,
    //            tracker->objects[i].age,
    //            tracker->objects[i].hits,
    //            roi_info,
    //            trajectory_str);
    // }

    // Process events after all tracking updates
    process_events(tracker);
}

// Free tracker resources
void free_tracker(Tracker *tracker)
{
    if (tracker)
    {
        if (tracker->objects)
        {
            free(tracker->objects);
        }
        free(tracker);
    }
}