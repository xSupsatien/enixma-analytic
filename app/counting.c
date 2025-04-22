#include "counting.h"
#include "fastcgi.h"
#include "event.h"
#include "imwrite.h"
#include "incident.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <syslog.h>
#include <time.h>
#include <string.h>

CountingSystem *counting_system = NULL;

// Add this global variable to track the last reset day
static int last_reset_day = -1;

// Global variable to track the last backup time
static time_t last_backup_time = 0;
static int backup_interval_seconds = 1; // Default 1 minutes (60 seconds)

int daily_vehicle_count[DAILY_ARRAY_SIZE] = {0};
int weekly_vehicle_count[WEEKLY_ARRAY_SIZE] = {0};

double daily_vehicle_pcu[DAILY_ARRAY_SIZE] = {0.0f};
double weekly_vehicle_pcu[WEEKLY_ARRAY_SIZE] = {0.0f};

double average_speed[1] = {0};
double daily_average_speed[DAILY_ARRAY_SIZE] = {0};
double weekly_average_speed[WEEKLY_ARRAY_SIZE] = {0};

// bool firstTime = true;

// Add these variables to track cleaning intervals
static time_t last_velocity_clean_time = 0;
static int velocity_clean_interval_seconds = 300; // Clean every 5 minutes instead of every backup

static bool initialize_line_counters(MultiLaneLine *line, int num_lanes, int num_classes)
{
    line->up_counts = (int *)calloc(num_classes * num_lanes, sizeof(int));
    line->down_counts = (int *)calloc(num_classes * num_lanes, sizeof(int));
    line->timestamps = (gint64 *)calloc(num_lanes, sizeof(gint64));

    if (!line->up_counts || !line->down_counts || !line->timestamps)
    {
        free(line->up_counts);
        free(line->down_counts);
        free(line->timestamps);
        return false;
    }
    return true;
}

bool is_segment_crossed(Point *p1, Point *p2, LinePoint *seg_start, LinePoint *seg_end, int *lane_id_out)
{
    // Parameter lane_id_out is used for future extension but not used now
    (void)lane_id_out; // Suppress unused parameter warning

    float s1_x = seg_end->x - seg_start->x;
    float s1_y = seg_end->y - seg_start->y;
    float s2_x = p2->x - p1->x;
    float s2_y = p2->y - p1->y;

    float denominator = (-s2_x * s1_y + s1_x * s2_y);
    const float small_value = 1e-6f;
    if (fabsf(denominator) < small_value)
        return false; // Lines are parallel

    float s = (-s1_y * (seg_start->x - p1->x) + s1_x * (seg_start->y - p1->y)) / denominator;
    float t = (s2_x * (seg_start->y - p1->y) - s2_y * (seg_start->x - p1->x)) / denominator;

    return (s >= 0 && s <= 1 && t >= 0 && t <= 1);
}

int get_crossing_direction(Point *p1, Point *p2, LinePoint *seg_start, LinePoint *seg_end)
{
    float cross_product = (seg_end->x - seg_start->x) * (p2->y - p1->y) -
                          (seg_end->y - seg_start->y) * (p2->x - p1->x);
    return (cross_product > 0) ? 1 : -1; // 1 for down, -1 for up
}

// Function to reset all counters in the system
void reset_all_counters(CountingSystem *system)
{
    if (!system)
        return;

    // Reset first line counters
    if (system->line1.up_counts && system->line1.down_counts)
    {
        memset(system->line1.up_counts, 0, system->num_classes * system->line1.num_lanes * sizeof(int));
        memset(system->line1.down_counts, 0, system->num_classes * system->line1.num_lanes * sizeof(int));
    }

    // Reset second line counters if enabled
    if (system->use_second_line && system->line2.up_counts && system->line2.down_counts)
    {
        memset(system->line2.up_counts, 0, system->num_classes * system->line2.num_lanes * sizeof(int));
        memset(system->line2.down_counts, 0, system->num_classes * system->line2.num_lanes * sizeof(int));
    }

    // Note: We don't reset the velocity buffer here since we want to keep historical velocity data

    // syslog(LOG_INFO, "All counters have been reset at midnight");
}

// Function to check if midnight has passed and reset counters if needed
// This function should be called regularly from the main application loop
bool check_midnight_reset(CountingSystem *system)
{
    if (!system)
        return false;

    // Get current time
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    // Get the current day
    int current_day = local_time->tm_mday;

    // Check if it's a new day compared to our last reset
    if (last_reset_day != current_day)
    {
        // Create a backup before resetting, using the fixed filename
        const char *backup_filename = "/usr/local/packages/enixma_analytic/localdata/counts_backup.json";
        save_counting_data(system, backup_filename);

        // Save the vehicle count data in the new format
        const char *vehicle_counts_filename = "/usr/local/packages/enixma_analytic/localdata/vehicle_counts.json";
        save_vehicle_count_data(system, vehicle_counts_filename);

        // Save the vehicle pcu data in the new format
        const char *vehicle_pcu_filename = "/usr/local/packages/enixma_analytic/localdata/vehicle_pcu.json";
        save_vehicle_pcu_data(system, vehicle_pcu_filename);

        // Clean velocity records older than 24 hours (86400000000 microseconds)
        // This keeps a day's worth of data across the midnight boundary for continuity
        clean_velocity_buffer(system, 86400000000);

        // Clear the daily vehicle count array
        memset(daily_vehicle_count, 0, sizeof(daily_vehicle_count));
        shift_array_left(weekly_vehicle_count, WEEKLY_ARRAY_SIZE);

        memset(daily_vehicle_pcu, 0, sizeof(daily_vehicle_pcu));
        shift_array_left_double(weekly_vehicle_pcu, WEEKLY_ARRAY_SIZE);

        memset(daily_average_speed, 0, sizeof(daily_average_speed));
        shift_array_left_double(weekly_average_speed, WEEKLY_ARRAY_SIZE);

        // Reset all counters
        reset_all_counters(system);

        // Update the last reset day
        last_reset_day = current_day;

        // Reset velocity clean timer to prevent immediate cleaning after reset
        last_velocity_clean_time = now;

        return true;
    }

    return false;
}

CountingSystem *init_counting_system(int num_classes, int num_lanes, LinePoint *points)
{
    if (num_lanes > MAX_LANES || !points || num_lanes <= 0)
        return NULL;

    // Initialize last_reset_day when the system is first created
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    last_reset_day = local_time->tm_mday;

    // Initialize the last backup time and velocity clean time
    last_backup_time = now;
    last_velocity_clean_time = now;

    CountingSystem *system = (CountingSystem *)malloc(sizeof(CountingSystem));
    if (!system)
        return NULL;

    // Rest of initialization code remains the same
    system->num_classes = num_classes;
    system->use_second_line = false;

    // Initialize direction flags to false (count up by default)
    system->line1_direction = false;
    system->line2_direction = false;

    // Initialize first line
    system->line1.num_lanes = num_lanes;
    system->line1.num_points = num_lanes + 1;

    if (!initialize_line_counters(&system->line1, num_lanes, num_classes))
    {
        free(system);
        return NULL;
    }

    for (int i = 0; i < system->line1.num_points; i++)
    {
        system->line1.points[i] = points[i];
    }

    // Initialize second line structures but don't enable it
    system->line2.num_lanes = 0;
    system->line2.num_points = 0;
    system->line2.up_counts = NULL;
    system->line2.down_counts = NULL;
    system->line2.timestamps = NULL;

    // Initialize velocity buffer
    system->velocity_buffer_count = 0;
    system->velocity_buffer_index = 0;

    return system;
}

void update_line_points(CountingSystem *system, LineId line_id, LinePoint *points, int num_points)
{
    if (!system || !points || num_points > MAX_SEGMENTS || num_points <= 1)
    {
        return;
    }

    MultiLaneLine *line = (line_id == LINE_1) ? &system->line1 : &system->line2;

    // For second line, initialize if not already done
    if (line_id == LINE_2 && !system->use_second_line)
    {
        if (!initialize_line_counters(line, num_points - 1, system->num_classes))
        {
            return;
        }
        system->use_second_line = true;
    }

    line->num_points = num_points;
    line->num_lanes = num_points - 1;

    for (int i = 0; i < num_points; i++)
    {
        line->points[i] = points[i];
    }
}

bool resize_line_lanes(CountingSystem *system, LineId line_id, int new_lane_count)
{
    if (!system || new_lane_count <= 0 || new_lane_count > MAX_LANES)
    {
        return false;
    }

    MultiLaneLine *line = (line_id == LINE_1) ? &system->line1 : &system->line2;

    // If second line isn't initialized and we're trying to resize it, initialize it
    if (line_id == LINE_2 && !system->use_second_line)
    {
        if (!initialize_line_counters(line, new_lane_count, system->num_classes))
        {
            return false;
        }
        system->use_second_line = true;
        line->num_lanes = new_lane_count;
        line->num_points = new_lane_count + 1;
        return true;
    }

    // Allocate new arrays
    int *new_up_counts = (int *)calloc(system->num_classes * new_lane_count, sizeof(int));
    int *new_down_counts = (int *)calloc(system->num_classes * new_lane_count, sizeof(int));
    gint64 *new_timestamps = (gint64 *)calloc(new_lane_count, sizeof(gint64));

    if (!new_up_counts || !new_down_counts || !new_timestamps)
    {
        free(new_up_counts);
        free(new_down_counts);
        free(new_timestamps);
        return false;
    }

    // Copy existing data
    int min_lanes = (new_lane_count < line->num_lanes) ? new_lane_count : line->num_lanes;

    for (int class_idx = 0; class_idx < system->num_classes; class_idx++)
    {
        for (int lane = 0; lane < min_lanes; lane++)
        {
            int old_idx = class_idx * line->num_lanes + lane;
            int new_idx = class_idx * new_lane_count + lane;
            new_up_counts[new_idx] = line->up_counts[old_idx];
            new_down_counts[new_idx] = line->down_counts[old_idx];
        }
    }

    // Copy timestamps
    for (int i = 0; i < min_lanes; i++)
    {
        new_timestamps[i] = line->timestamps[i];
    }

    // Free old arrays
    free(line->up_counts);
    free(line->down_counts);
    free(line->timestamps);

    // Update with new arrays
    line->up_counts = new_up_counts;
    line->down_counts = new_down_counts;
    line->timestamps = new_timestamps;
    line->num_lanes = new_lane_count;
    line->num_points = new_lane_count + 1;

    return true;
}

void update_counting(CountingSystem *system, TrackedObject *obj)
{
    if (!system || !obj || obj->trajectory_count < 2 || obj->counted)
    {
        return;
    }

    Point *prev = &obj->trajectory[obj->trajectory_count - 2];
    Point *curr = &obj->trajectory[obj->trajectory_count - 1];
    int class_id = obj->class_id;

    // Check first line
    for (int i = 0; i < system->line1.num_lanes; i++)
    {
        int lane_id;
        if (is_segment_crossed(prev, curr, &system->line1.points[i],
                               &system->line1.points[i + 1], &lane_id))
        {
            system->line1.timestamps[i] = g_get_monotonic_time();
            int direction = get_crossing_direction(prev, curr,
                                                   &system->line1.points[i], &system->line1.points[i + 1]);

            if (class_id >= 0 && class_id < system->num_classes)
            {
                int idx = class_id * system->line1.num_lanes + i;

                // If line1_direction is true, we want to count DOWN objects only
                // If line1_direction is false, we want to count UP objects only
                bool is_desired_direction = (direction > 0 && system->line1_direction) ||
                                            (direction < 0 && !system->line1_direction);

                int type = 0;
                if (direction > 0)
                { // Moving DOWN
                    if (system->line1_direction)
                    { // We want DOWN
                        system->line1.down_counts[idx]++;
                        // syslog(LOG_INFO, "Line 1 Lane %d - Class %d object %d moving DOWN. Count: %d",
                        //        i + 1, class_id, obj->track_id, system->line1.down_counts[idx]);
                        obj->counted = true;

                        // Add velocity record for this object
                        update_velocity(obj, frame_time, pixels_per_meter, context.resolution.widthFrameHD, context.resolution.heightFrameHD);
                        add_velocity_record(system, obj->speed_kmh, class_id);
                        send_event_counting(app_data_counting, context.label.labels[class_id], obj->speed_kmh, 1, i + 1, "down");

                        // Condition 1: Speed > 120 km/h (any lane, any class)
                        if (obj->speed_kmh > first_overspeed && first_overspeed_received)
                        {
                            type = 8;
                            // syslog(LOG_INFO, "High speed detected: %f km/h, Line 1 Lane %d - Class %d object %d",
                            //        obj->speed_kmh, i + 1, class_id, obj->track_id);
                        }

                        // Check if system has more than one lane before applying lane-specific conditions
                        if (system->line1.num_lanes > 1)
                        {
                            // Condition 2: Truck in right lane
                            if (((class_id == 2 || class_id == 6) && i == 0) && first_truckright)
                            {
                                type = 7;
                                // syslog(LOG_INFO, "Line 1 Lane 1 - Class %d object %d moving DOWN. Count: %d",
                                //        class_id, obj->track_id, system->line1.down_counts[idx]);
                            }

                            // Condition 3: (Speed < 90 OR Speed > 120) in right lane
                            if (((obj->speed_kmh < first_limitspeed.min || obj->speed_kmh > first_limitspeed.max) && i == 0) && first_limitspeed_received)
                            {
                                type = 9;
                                // syslog(LOG_INFO, "Abnormal speed in Lane 1: %f km/h, Class %d object %d",
                                //        obj->speed_kmh, class_id, obj->track_id);
                            }
                        }
                    }
                    else
                    {
                        if (first_wrongway)
                        {
                            type = 6;
                            // syslog(LOG_INFO, "Line 1 Lane %d - Class %d object %d moving DOWN but we're counting UP. Ignoring.",
                            //        i + 1, class_id, obj->track_id);
                        }
                    }
                }
                else
                { // Moving UP
                    if (!system->line1_direction)
                    { // We want UP
                        system->line1.up_counts[idx]++;
                        // syslog(LOG_INFO, "Line 1 Lane %d - Class %d object %d moving UP. Count: %d",
                        //        i + 1, class_id, obj->track_id, system->line1.up_counts[idx]);
                        obj->counted = true;

                        // Add velocity record for this object
                        update_velocity(obj, frame_time, pixels_per_meter, context.resolution.widthFrameHD, context.resolution.heightFrameHD);
                        add_velocity_record(system, obj->speed_kmh, class_id);
                        send_event_counting(app_data_counting, context.label.labels[class_id], obj->speed_kmh, 1, i + 1, "up");

                        // Condition 1: Speed > 120 km/h (any lane, any class)
                        if (obj->speed_kmh > first_overspeed && first_overspeed_received)
                        {
                            type = 8;
                            // syslog(LOG_INFO, "High speed detected: %f km/h, Line 1 Lane %d - Class %d object %d",
                            //        obj->speed_kmh, i + 1, class_id, obj->track_id);
                        }

                        // Check if system has more than one lane before applying lane-specific conditions
                        if (system->line1.num_lanes > 1)
                        {
                            // Condition 2: Truck in right lane
                            if (((class_id == 2 || class_id == 6) && i == system->line1.num_lanes - 1) && first_truckright)
                            {
                                type = 7;
                                // syslog(LOG_INFO, "Line 1 Lane %d - Class %d object %d moving DOWN. Count: %d",
                                //        system->line1.num_lanes, class_id, obj->track_id, system->line1.down_counts[idx]);
                            }

                            // Condition 3: (Speed < 90 OR Speed > 120) in right lane
                            if (((obj->speed_kmh < first_limitspeed.min || obj->speed_kmh > first_limitspeed.max) && i == system->line1.num_lanes - 1) && first_limitspeed_received)
                            {
                                type = 9;
                                // syslog(LOG_INFO, "Abnormal speed in Lane %d: %f km/h, Class %d object %d",
                                //        system->line1.num_lanes, obj->speed_kmh, class_id, obj->track_id);
                            }
                        }
                    }
                    else
                    {
                        if (first_wrongway)
                        {
                            type = 6;
                            // syslog(LOG_INFO, "Line 1 Lane %d - Class %d object %d moving UP but we're counting DOWN. Ignoring.",
                            //        i + 1, class_id, obj->track_id);
                        }
                    }
                }

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
                        // syslog(LOG_INFO, "Line 1 Event: %s - Class %s speed %.2f km.h",
                        //        incident_types[type], context.label.labels[obj->class_id], obj->speed_kmh);

                        imwrite(filename, context.addresses.ppOutputAddrHD);
                        send_event_incidents(app_data_incidents, context.label.labels[obj->class_id], incident_types[type], 1, obj->speed_kmh, filename);
                    }
                }

                if (is_desired_direction)
                {
                    obj->counted = true;
                    return;
                }
            }
        }
    }

    // Check second line if enabled
    if (system->use_second_line && !obj->counted)
    {
        for (int i = 0; i < system->line2.num_lanes; i++)
        {
            int lane_id_out;
            if (is_segment_crossed(prev, curr, &system->line2.points[i],
                                   &system->line2.points[i + 1], &lane_id_out))
            {
                system->line2.timestamps[i] = g_get_monotonic_time();
                int direction = get_crossing_direction(prev, curr,
                                                       &system->line2.points[i], &system->line2.points[i + 1]);

                if (class_id >= 0 && class_id < system->num_classes)
                {
                    int idx = class_id * system->line2.num_lanes + i;

                    // If line2_direction is true, we want to count DOWN objects only
                    // If line2_direction is false, we want to count UP objects only
                    bool is_desired_direction = (direction > 0 && system->line2_direction) ||
                                                (direction < 0 && !system->line2_direction);

                    int type = 0;
                    if (direction > 0)
                    {
                        // Moving DOWN
                        if (system->line2_direction)
                        { // We want DOWN
                            system->line2.down_counts[idx]++;
                            // syslog(LOG_INFO, "Line 2 Lane %d - Class %d object %d moving DOWN. Count: %d",
                            //        i, class_id, obj->track_id, system->line2.down_counts[idx]);
                            obj->counted = true;

                            // Add velocity record for this object
                            update_velocity(obj, frame_time, pixels_per_meter, context.resolution.widthFrameHD, context.resolution.heightFrameHD);
                            add_velocity_record(system, obj->speed_kmh, class_id);
                            send_event_counting(app_data_counting, context.label.labels[class_id], obj->speed_kmh, 2, i + 1, "down");

                            // Condition 1: Speed > 120 km/h (any lane, any class)
                            if (obj->speed_kmh > second_overspeed && second_overspeed_received)
                            {
                                type = 8;
                                // syslog(LOG_INFO, "High speed detected: %f km/h, Line 2 Lane %d - Class %d object %d",
                                //        obj->speed_kmh, i + 1, class_id, obj->track_id);
                            }

                            // Check if system has more than one lane before applying lane-specific conditions
                            if (system->line2.num_lanes > 1)
                            {
                                // Condition 2: Truck in right lane
                                if (((class_id == 2 || class_id == 6) && i == 0) && second_truckright)
                                {
                                    type = 7;
                                    // syslog(LOG_INFO, "Line 2 Lane 1 - Class %d object %d moving DOWN. Count: %d",
                                    //        class_id, obj->track_id, system->line2.down_counts[idx]);
                                }

                                // Condition 3: (Speed < 90 OR Speed > 120) in right lane
                                if (((obj->speed_kmh < second_limitspeed.min || obj->speed_kmh > second_limitspeed.max) && i == 0) && second_limitspeed_received)
                                {
                                    type = 9;
                                    // syslog(LOG_INFO, "Abnormal speed in Lane 1: %f km/h, Class %d object %d",
                                    //        obj->speed_kmh, class_id, obj->track_id);
                                }
                            }
                        }
                        else
                        {
                            if (second_wrongway)
                            {
                                type = 6;
                                // syslog(LOG_INFO, "Line 2 Lane %d - Class %d object %d moving DOWN but we're counting UP. Ignoring.",
                                //        i, class_id, obj->track_id);
                            }
                        }
                    }
                    else
                    { // Moving UP
                        if (!system->line2_direction)
                        { // We want UP
                            system->line2.up_counts[idx]++;
                            // syslog(LOG_INFO, "Line 2 Lane %d - Class %d object %d moving UP. Count: %d",
                            //        i, class_id, obj->track_id, system->line2.up_counts[idx]);
                            obj->counted = true;

                            // Add velocity record for this object
                            update_velocity(obj, frame_time, pixels_per_meter, context.resolution.widthFrameHD, context.resolution.heightFrameHD);
                            add_velocity_record(system, obj->speed_kmh, class_id);
                            send_event_counting(app_data_counting, context.label.labels[class_id], obj->speed_kmh, 2, i + 1, "up");

                            // Condition 1: Speed > 120 km/h (any lane, any class)
                            if (obj->speed_kmh > second_overspeed && second_overspeed_received)
                            {
                                type = 8;
                                // syslog(LOG_INFO, "High speed detected: %f km/h, Line 2 Lane %d - Class %d object %d",
                                //        obj->speed_kmh, i + 1, class_id, obj->track_id);
                            }

                            // Check if system has more than one lane before applying lane-specific conditions
                            if (system->line2.num_lanes > 1)
                            {
                                // Condition 2: Truck in right lane
                                if (((class_id == 2 || class_id == 6) && i == system->line2.num_lanes - 1) && second_truckright)
                                {
                                    type = 7;
                                    // syslog(LOG_INFO, "Line 2 Lane %d - Class %d object %d moving DOWN. Count: %d",
                                    //        system->line2.num_lanes, class_id, obj->track_id, system->line2.down_counts[idx]);
                                }

                                // Condition 3: (Speed < 90 OR Speed > 120) in right lane
                                if (((obj->speed_kmh < second_limitspeed.min || obj->speed_kmh > second_limitspeed.max) && i == system->line2.num_lanes - 1) && second_limitspeed_received)
                                {
                                    type = 9;
                                    // syslog(LOG_INFO, "Abnormal speed in Lane %d: %f km/h, Class %d object %d",
                                    //        system->line2.num_lanes, obj->speed_kmh, class_id, obj->track_id);
                                }
                            }
                        }
                        else
                        {
                            if (second_wrongway)
                            {
                                type = 6;
                                // syslog(LOG_INFO, "Line 2 Lane %d - Class %d object %d moving UP but we're counting DOWN. Ignoring.",
                                //        i, class_id, obj->track_id);
                            }
                        }
                    }

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
                            // syslog(LOG_INFO, "Line 2 Event: %s - Class %s speed %.2f km.h",
                            //        incident_types[type], context.label.labels[obj->class_id], obj->speed_kmh);

                            imwrite(filename, context.addresses.ppOutputAddrHD);
                            send_event_incidents(app_data_incidents, context.label.labels[obj->class_id], incident_types[type], 2, obj->speed_kmh, filename);
                        }
                    }

                    if (is_desired_direction)
                    {
                        obj->counted = true;
                        return;
                    }
                }
            }
        }
    }
}

void get_lane_counts(CountingSystem *system, LineId line_id, int class_id, int lane_id,
                     int *up_count, int *down_count)
{
    MultiLaneLine *line = (line_id == LINE_1) ? &system->line1 : &system->line2;

    if (!system || class_id >= system->num_classes ||
        lane_id >= line->num_lanes ||
        (line_id == LINE_2 && !system->use_second_line))
    {
        *up_count = 0;
        *down_count = 0;
        return;
    }

    int idx = class_id * line->num_lanes + lane_id;
    *up_count = line->up_counts[idx];
    *down_count = line->down_counts[idx];
}

void free_counting_system(CountingSystem *system)
{
    if (system)
    {
        // Free first line resources
        free(system->line1.up_counts);
        free(system->line1.down_counts);
        free(system->line1.timestamps);

        // Free second line resources if initialized
        if (system->use_second_line)
        {
            free(system->line2.up_counts);
            free(system->line2.down_counts);
            free(system->line2.timestamps);
        }

        free(system);
    }
}

// Function to add a velocity record to the buffer
void add_velocity_record(CountingSystem *system, float velocity, int class_id)
{
    if (!system)
        return;

    // Get current timestamp
    gint64 current_time = g_get_monotonic_time(); // microseconds

    // If buffer is getting too full (e.g., more than 95% capacity),
    // clean records older than 2 hours even if it's not time for regular cleaning
    if (system->velocity_buffer_count > (HOURLY_VELOCITY_BUFFER_SIZE * 0.95))
    {
        clean_velocity_buffer(system, 7200000000); // 2 hours in microseconds
        // syslog(LOG_INFO, "Emergency velocity buffer cleaning due to near-capacity condition");
    }

    // Add the record to the circular buffer
    int idx = system->velocity_buffer_index;
    system->velocity_buffer[idx].velocity = velocity;
    system->velocity_buffer[idx].timestamp = current_time;
    system->velocity_buffer[idx].class_id = class_id;

    // Update buffer index and count
    system->velocity_buffer_index = (system->velocity_buffer_index + 1) % HOURLY_VELOCITY_BUFFER_SIZE;
    if (system->velocity_buffer_count < HOURLY_VELOCITY_BUFFER_SIZE)
        system->velocity_buffer_count++;
}

// Function to calculate average velocity over a time window
float get_average_velocity(CountingSystem *system, int time_window_ms, int class_id)
{
    if (!system || system->velocity_buffer_count == 0)
        return 0.0f;

    gint64 current_time = g_get_monotonic_time();
    gint64 time_window_us = (gint64)time_window_ms * 1000; // Convert ms to microseconds
    gint64 cutoff_time = current_time - time_window_us;

    float sum_velocity = 0.0f;
    int count = 0;

    // Iterate through the buffer
    for (int i = 0; i < system->velocity_buffer_count; i++)
    {
        int idx = (system->velocity_buffer_index - 1 - i + HOURLY_VELOCITY_BUFFER_SIZE) % HOURLY_VELOCITY_BUFFER_SIZE;

        // Skip if record is outside the time window
        if (system->velocity_buffer[idx].timestamp < cutoff_time)
            continue;

        // Skip if not the requested class_id (or include all classes if class_id < 0)
        if (class_id >= 0 && system->velocity_buffer[idx].class_id != class_id)
            continue;

        sum_velocity += system->velocity_buffer[idx].velocity;
        count++;
    }

    return (count > 0) ? (sum_velocity / count) : 0.0f;
}

// Function to clean old velocity records from the buffer
void clean_velocity_buffer(CountingSystem *system, gint64 max_age_us)
{
    if (!system || system->velocity_buffer_count == 0)
        return;

    gint64 current_time = g_get_monotonic_time(); // Current time in microseconds
    gint64 cutoff_time = current_time - max_age_us;

    int read_idx = 0;
    int write_idx = 0;
    int records_removed = 0;

    // Temporary buffer to hold valid records
    VelocityRecord temp_buffer[HOURLY_VELOCITY_BUFFER_SIZE];

    // Iterate through all records and keep only the recent ones
    for (int i = 0; i < system->velocity_buffer_count; i++)
    {
        read_idx = (system->velocity_buffer_index - system->velocity_buffer_count + i + HOURLY_VELOCITY_BUFFER_SIZE) % HOURLY_VELOCITY_BUFFER_SIZE;

        if (system->velocity_buffer[read_idx].timestamp >= cutoff_time)
        {
            // This record is recent enough, keep it
            temp_buffer[write_idx++] = system->velocity_buffer[read_idx];
        }
        else
        {
            records_removed++;
        }
    }

    // If any records were removed, update the buffer
    if (records_removed > 0)
    {
        // Copy valid records back to the buffer (at the beginning)
        for (int i = 0; i < write_idx; i++)
        {
            system->velocity_buffer[i] = temp_buffer[i];
        }

        // Update buffer state
        system->velocity_buffer_count = write_idx;
        system->velocity_buffer_index = write_idx % HOURLY_VELOCITY_BUFFER_SIZE;

        // syslog(LOG_INFO, "Cleaned velocity buffer: removed %d old records, kept %d recent records",
        //        records_removed, write_idx);
    }
}

// Function to check if it's time for a periodic backup
bool check_periodic_backup(CountingSystem *system)
{
    if (!system)
        return false;

    // Get current time
    time_t now = time(NULL);

    // If first run, initialize last_backup_time and last_velocity_clean_time
    if (last_backup_time == 0)
    {
        last_backup_time = now;
        last_velocity_clean_time = now; // Initialize velocity cleaning timer
        return false;
    }

    // Check if enough time has passed for a backup
    if (difftime(now, last_backup_time) >= backup_interval_seconds)
    {
        // Save the counting
        const char *backup_filename = "/usr/local/packages/enixma_analytic/localdata/counts_backup.json";
        save_counting_data(system, backup_filename);

        // Save the vehicle count data in the new format
        const char *vehicle_counts_filename = "/usr/local/packages/enixma_analytic/localdata/vehicle_counts.json";
        save_vehicle_count_data(system, vehicle_counts_filename);

        const char *vehicle_counts_daily_filename = "/usr/local/packages/enixma_analytic/localdata/daily_vehicle_count.json";
        save_daily_vehicle_count_data(system, vehicle_counts_daily_filename);

        const char *vehicle_counts_weekly_filename = "/usr/local/packages/enixma_analytic/localdata/weekly_vehicle_count.json";
        save_weekly_vehicle_count_data(system, vehicle_counts_weekly_filename);

        // Save the vehicle pcu data in the new format
        const char *vehicle_pcu_filename = "/usr/local/packages/enixma_analytic/localdata/vehicle_pcu.json";
        save_vehicle_pcu_data(system, vehicle_pcu_filename);

        const char *vehicle_pcu_daily_filename = "/usr/local/packages/enixma_analytic/localdata/daily_vehicle_pcu.json";
        save_daily_vehicle_pcu_data(system, vehicle_pcu_daily_filename);

        const char *vehicle_pcu_weekly_filename = "/usr/local/packages/enixma_analytic/localdata/weekly_vehicle_pcu.json";
        save_weekly_vehicle_pcu_data(system, vehicle_pcu_weekly_filename);

        // Save velocity data
        const char *average_speed_filename = "/usr/local/packages/enixma_analytic/localdata/average_speed.json";
        save_average_speed_data(system, average_speed_filename);

        const char *average_speed_daily_filename = "/usr/local/packages/enixma_analytic/localdata/daily_average_speed.json";
        save_daily_average_speed_data(system, average_speed_daily_filename);

        const char *average_speed_weekly_filename = "/usr/local/packages/enixma_analytic/localdata/weekly_average_speed.json";
        save_weekly_average_speed_data(system, average_speed_weekly_filename);

        // Clean velocity buffer hourly, not on every backup
        // Only clean if it's been at least an hour since the last cleaning
        if (difftime(now, last_velocity_clean_time) >= velocity_clean_interval_seconds)
        {
            // Clean velocity records older than 1 hour (3600000000 microseconds)
            clean_velocity_buffer(system, 3600000000);
            last_velocity_clean_time = now;

            // syslog(LOG_INFO, "Performed scheduled velocity buffer cleaning");
        }

        // Update the last backup time
        last_backup_time = now;
        return true;
    }

    return false;
}

// Function to convert counting data to JSON format
json_t *counting_data_to_json(CountingSystem *system)
{
    if (!system)
        return NULL;

    json_t *root = json_object();

    // Add system-wide properties
    json_object_set_new(root, "num_classes", json_integer(system->num_classes));
    json_object_set_new(root, "use_second_line", json_boolean(system->use_second_line));
    json_object_set_new(root, "line1_direction", json_boolean(system->line1_direction));
    json_object_set_new(root, "line2_direction", json_boolean(system->line2_direction));

    // Add velocity information
    float avg_velocity = get_average_velocity(system, 3600000, -1); // Last hour, all classes
    json_object_set_new(root, "average_velocity_kmh", json_real(avg_velocity));

    // Create class-specific velocity averages
    json_t *class_velocity = json_array();
    for (int i = 0; i < system->num_classes; i++)
    {
        float class_avg = get_average_velocity(system, 3600000, i);
        json_array_append_new(class_velocity, json_real(class_avg));
    }
    json_object_set_new(root, "class_velocities", class_velocity);

    // Save the entire velocity buffer
    json_t *velocity_buffer = json_array();

    // If buffer is not full, we only need to save the records we have
    int records_to_save = system->velocity_buffer_count;

    // We'll store them in chronological order (oldest first)
    for (int i = 0; i < records_to_save; i++)
    {
        // Calculate the index in the circular buffer
        int idx = (system->velocity_buffer_index - records_to_save + i + HOURLY_VELOCITY_BUFFER_SIZE) % HOURLY_VELOCITY_BUFFER_SIZE;

        // Create a record object
        json_t *record = json_object();
        json_object_set_new(record, "velocity", json_real(system->velocity_buffer[idx].velocity));
        json_object_set_new(record, "timestamp", json_integer(system->velocity_buffer[idx].timestamp));
        json_object_set_new(record, "class_id", json_integer(system->velocity_buffer[idx].class_id));

        // Add to the buffer array
        json_array_append_new(velocity_buffer, record);
    }

    json_object_set_new(root, "velocity_buffer", velocity_buffer);

    // Create first line data
    json_t *line1 = json_object();
    json_object_set_new(line1, "num_lanes", json_integer(system->line1.num_lanes));

    // Add line1 points
    json_t *line1_points = json_array();
    for (int i = 0; i < system->line1.num_points; i++)
    {
        json_t *point = json_object();
        json_object_set_new(point, "x", json_real(system->line1.points[i].x));
        json_object_set_new(point, "y", json_real(system->line1.points[i].y));
        json_array_append_new(line1_points, point);
    }
    json_object_set_new(line1, "points", line1_points);

    // Add line1 counts for each class and lane
    json_t *line1_up = json_array();
    json_t *line1_down = json_array();

    for (int class_idx = 0; class_idx < system->num_classes; class_idx++)
    {
        json_t *class_up = json_array();
        json_t *class_down = json_array();

        for (int lane = 0; lane < system->line1.num_lanes; lane++)
        {
            int idx = class_idx * system->line1.num_lanes + lane;
            json_array_append_new(class_up, json_integer(system->line1.up_counts[idx]));
            json_array_append_new(class_down, json_integer(system->line1.down_counts[idx]));
        }

        json_array_append_new(line1_up, class_up);
        json_array_append_new(line1_down, class_down);
    }

    json_object_set_new(line1, "up_counts", line1_up);
    json_object_set_new(line1, "down_counts", line1_down);

    // Add line1 to root
    json_object_set_new(root, "line1", line1);

    // Handle second line if enabled
    if (system->use_second_line)
    {
        json_t *line2 = json_object();
        json_object_set_new(line2, "num_lanes", json_integer(system->line2.num_lanes));

        // Add line2 points
        json_t *line2_points = json_array();
        for (int i = 0; i < system->line2.num_points; i++)
        {
            json_t *point = json_object();
            json_object_set_new(point, "x", json_real(system->line2.points[i].x));
            json_object_set_new(point, "y", json_real(system->line2.points[i].y));
            json_array_append_new(line2_points, point);
        }
        json_object_set_new(line2, "points", line2_points);

        // Add line2 counts for each class and lane
        json_t *line2_up = json_array();
        json_t *line2_down = json_array();

        for (int class_idx = 0; class_idx < system->num_classes; class_idx++)
        {
            json_t *class_up = json_array();
            json_t *class_down = json_array();

            for (int lane = 0; lane < system->line2.num_lanes; lane++)
            {
                int idx = class_idx * system->line2.num_lanes + lane;
                json_array_append_new(class_up, json_integer(system->line2.up_counts[idx]));
                json_array_append_new(class_down, json_integer(system->line2.down_counts[idx]));
            }

            json_array_append_new(line2_up, class_up);
            json_array_append_new(line2_down, class_down);
        }

        json_object_set_new(line2, "up_counts", line2_up);
        json_object_set_new(line2, "down_counts", line2_down);

        // Add line2 to root
        json_object_set_new(root, "line2", line2);
    }

    // Add timestamp for when the backup was created
    time_t now = time(NULL);
    json_object_set_new(root, "backup_timestamp", json_integer(now));

    // Add human readable timestamp
    struct tm *local_time = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
    json_object_set_new(root, "backup_time", json_string(time_str));

    return root;
}

// Function to save counting data to a file
bool save_counting_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    // Convert counting data to JSON
    json_t *json_data = counting_data_to_json(system);
    if (!json_data)
    {
        syslog(LOG_ERR, "Failed to convert counting data to JSON");
        return false;
    }

    // Save JSON to file
    int result = json_dump_file(json_data, filename, JSON_COMPACT | JSON_ENSURE_ASCII);

    json_decref(json_data);

    if (result == 0)
    {
        // syslog(LOG_INFO, "Counting data successfully saved to %s", filename);
        return true;
    }
    else
    {
        syslog(LOG_ERR, "Failed to save counting data to %s", filename);
        return false;
    }
}

// Function to load counting data from a file
bool load_counting_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    // Load JSON from file
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root)
        return false;

    // Validate structure - should have num_classes
    json_t *num_classes_json = json_object_get(root, "num_classes");
    if (!json_is_integer(num_classes_json))
    {
        json_decref(root);
        return false;
    }

    int num_classes = json_integer_value(num_classes_json);
    if (num_classes != system->num_classes)
    {
        // We'll continue anyway and adapt as needed
    }

    // Get system-wide properties
    json_t *line1_direction_json = json_object_get(root, "line1_direction");
    if (json_is_boolean(line1_direction_json))
    {
        system->line1_direction = json_boolean_value(line1_direction_json);
    }

    json_t *line2_direction_json = json_object_get(root, "line2_direction");
    if (json_is_boolean(line2_direction_json))
    {
        system->line2_direction = json_boolean_value(line2_direction_json);
    }

    // Try to load velocity buffer if it exists
    json_t *velocity_buffer_json = json_object_get(root, "velocity_buffer");
    if (json_is_array(velocity_buffer_json))
    {
        // Reset current velocity buffer
        system->velocity_buffer_count = 0;
        system->velocity_buffer_index = 0;

        size_t buffer_size = json_array_size(velocity_buffer_json);
        size_t max_records = (buffer_size < HOURLY_VELOCITY_BUFFER_SIZE) ? buffer_size : HOURLY_VELOCITY_BUFFER_SIZE;

        for (size_t i = 0; i < max_records; i++)
        {
            json_t *record_json = json_array_get(velocity_buffer_json, i);
            if (json_is_object(record_json))
            {
                json_t *velocity_json = json_object_get(record_json, "velocity");
                json_t *timestamp_json = json_object_get(record_json, "timestamp");
                json_t *class_id_json = json_object_get(record_json, "class_id");

                if (json_is_real(velocity_json) && json_is_integer(timestamp_json) &&
                    json_is_integer(class_id_json))
                {
                    int idx = system->velocity_buffer_index;
                    system->velocity_buffer[idx].velocity = (float)json_real_value(velocity_json);
                    system->velocity_buffer[idx].timestamp = json_integer_value(timestamp_json);
                    system->velocity_buffer[idx].class_id = json_integer_value(class_id_json);

                    system->velocity_buffer_index = (system->velocity_buffer_index + 1) % HOURLY_VELOCITY_BUFFER_SIZE;
                    system->velocity_buffer_count++;
                }
            }
        }

        // syslog(LOG_INFO, "Loaded %d velocity records from backup", system->velocity_buffer_count);
    }

    // Update line1 data
    json_t *line1_json = json_object_get(root, "line1");
    if (!json_is_object(line1_json))
    {
        json_decref(root);
        return false;
    }

    // Get line1 lane count
    json_t *line1_lanes_json = json_object_get(line1_json, "num_lanes");
    if (json_is_integer(line1_lanes_json))
    {
        int file_lane_count = json_integer_value(line1_lanes_json);

        // If lane counts don't match, we might need to resize
        if (file_lane_count != system->line1.num_lanes)
        {
            // Attempt to resize the lanes to match the file
            if (file_lane_count > 0 && file_lane_count <= MAX_LANES)
            {
                resize_line_lanes(system, LINE_1, file_lane_count);
            }
        }
    }

    // Load line1 points data
    json_t *line1_points_json = json_object_get(line1_json, "points");
    if (json_is_array(line1_points_json))
    {
        size_t num_points = json_array_size(line1_points_json);

        // Only update points if the count matches or we can adapt
        if (num_points <= MAX_SEGMENTS && num_points > 1) // Need at least 2 points for a line
        {
            // Temporary array to hold the points
            LinePoint temp_points[MAX_SEGMENTS];

            // Extract all points first
            for (size_t i = 0; i < num_points && i < MAX_SEGMENTS; i++)
            {
                json_t *point_json = json_array_get(line1_points_json, i);
                if (json_is_object(point_json))
                {
                    json_t *x_json = json_object_get(point_json, "x");
                    json_t *y_json = json_object_get(point_json, "y");

                    if (json_is_real(x_json) && json_is_real(y_json))
                    {
                        temp_points[i].x = (float)json_real_value(x_json);
                        temp_points[i].y = (float)json_real_value(y_json);
                    }
                    else
                    {
                        // Default to zeros if values are not valid
                        temp_points[i].x = 0.0f;
                        temp_points[i].y = 0.0f;
                    }
                }
            }

            // Update the line points - this handles both the points and num_points
            update_line_points(system, LINE_1, temp_points, (int)num_points);
        }
    }

    // Get line1 up counts
    json_t *line1_up_json = json_object_get(line1_json, "up_counts");
    if (json_is_array(line1_up_json))
    {
        size_t json_class_size = json_array_size(line1_up_json);
        size_t min_class_count = (size_t)system->num_classes < json_class_size ? (size_t)system->num_classes : json_class_size;

        for (size_t class_idx = 0; class_idx < min_class_count; class_idx++)
        {
            json_t *class_up = json_array_get(line1_up_json, class_idx);
            if (json_is_array(class_up))
            {
                size_t json_lane_size = json_array_size(class_up);
                size_t min_lane_count = (size_t)system->line1.num_lanes < json_lane_size ? (size_t)system->line1.num_lanes : json_lane_size;

                for (size_t lane = 0; lane < min_lane_count; lane++)
                {
                    json_t *count = json_array_get(class_up, lane);
                    if (json_is_integer(count))
                    {
                        int idx = (int)class_idx * system->line1.num_lanes + (int)lane;
                        int value = json_integer_value(count);
                        system->line1.up_counts[idx] = value;
                    }
                }
            }
        }
    }

    // Get line1 down counts
    json_t *line1_down_json = json_object_get(line1_json, "down_counts");
    if (json_is_array(line1_down_json))
    {
        size_t json_class_size = json_array_size(line1_down_json);
        size_t min_class_count = (size_t)system->num_classes < json_class_size ? (size_t)system->num_classes : json_class_size;

        for (size_t class_idx = 0; class_idx < min_class_count; class_idx++)
        {
            json_t *class_down = json_array_get(line1_down_json, class_idx);
            if (json_is_array(class_down))
            {
                size_t json_lane_size = json_array_size(class_down);
                size_t min_lane_count = (size_t)system->line1.num_lanes < json_lane_size ? (size_t)system->line1.num_lanes : json_lane_size;

                for (size_t lane = 0; lane < min_lane_count; lane++)
                {
                    json_t *count = json_array_get(class_down, lane);
                    if (json_is_integer(count))
                    {
                        int idx = (int)class_idx * system->line1.num_lanes + (int)lane;
                        int value = json_integer_value(count);
                        system->line1.down_counts[idx] = value;
                    }
                }
            }
        }
    }

    // Update line2 data if it exists in both the file and the system
    json_t *line2_json = json_object_get(root, "line2");
    json_t *use_second_line_json = json_object_get(root, "use_second_line");

    bool file_has_line2 = json_is_object(line2_json) &&
                          json_is_boolean(use_second_line_json) &&
                          json_boolean_value(use_second_line_json);

    // Update system's use_second_line flag if file has line2 data
    if (file_has_line2)
    {
        // If file has line2 but system doesn't, try to enable it
        if (!system->use_second_line)
        {
            // Get line2 lane count
            json_t *line2_lanes_json = json_object_get(line2_json, "num_lanes");
            if (json_is_integer(line2_lanes_json))
            {
                int file_lane_count = json_integer_value(line2_lanes_json);
                if (file_lane_count > 0 && file_lane_count <= MAX_LANES)
                {
                    // Get the points to initialize line2
                    json_t *line2_points_json = json_object_get(line2_json, "points");
                    if (json_is_array(line2_points_json) && json_array_size(line2_points_json) >= 2)
                    {
                        // Extract points
                        size_t num_points = json_array_size(line2_points_json);
                        LinePoint temp_points[MAX_SEGMENTS];

                        for (size_t i = 0; i < num_points && i < MAX_SEGMENTS; i++)
                        {
                            json_t *point_json = json_array_get(line2_points_json, i);
                            if (json_is_object(point_json))
                            {
                                json_t *x_json = json_object_get(point_json, "x");
                                json_t *y_json = json_object_get(point_json, "y");

                                if (json_is_real(x_json) && json_is_real(y_json))
                                {
                                    temp_points[i].x = (float)json_real_value(x_json);
                                    temp_points[i].y = (float)json_real_value(y_json);
                                }
                                else
                                {
                                    temp_points[i].x = 0.0f;
                                    temp_points[i].y = 0.0f;
                                }
                            }
                        }

                        // Initialize line2
                        if (resize_line_lanes(system, LINE_2, file_lane_count))
                        {
                            update_line_points(system, LINE_2, temp_points, (int)num_points);
                            system->use_second_line = true;
                        }
                    }
                }
            }
        }
        else
        {
            // System already has line2, just check if we need to resize
            json_t *line2_lanes_json = json_object_get(line2_json, "num_lanes");
            if (json_is_integer(line2_lanes_json))
            {
                int file_lane_count = json_integer_value(line2_lanes_json);

                // If lane counts don't match, try to resize
                if (file_lane_count != system->line2.num_lanes &&
                    file_lane_count > 0 && file_lane_count <= MAX_LANES)
                {
                    resize_line_lanes(system, LINE_2, file_lane_count);
                }
            }

            // Load line2 points
            json_t *line2_points_json = json_object_get(line2_json, "points");
            if (json_is_array(line2_points_json))
            {
                size_t num_points = json_array_size(line2_points_json);

                if (num_points >= 2 && num_points <= MAX_SEGMENTS)
                {
                    LinePoint temp_points[MAX_SEGMENTS];

                    for (size_t i = 0; i < num_points; i++)
                    {
                        json_t *point_json = json_array_get(line2_points_json, i);
                        if (json_is_object(point_json))
                        {
                            json_t *x_json = json_object_get(point_json, "x");
                            json_t *y_json = json_object_get(point_json, "y");

                            if (json_is_real(x_json) && json_is_real(y_json))
                            {
                                temp_points[i].x = (float)json_real_value(x_json);
                                temp_points[i].y = (float)json_real_value(y_json);
                            }
                        }
                    }

                    update_line_points(system, LINE_2, temp_points, (int)num_points);
                }
            }
        }
    }

    // Now load line2 counts if system has line2 enabled
    if (system->use_second_line && file_has_line2)
    {
        // Get line2 up counts
        json_t *line2_up_json = json_object_get(line2_json, "up_counts");
        if (json_is_array(line2_up_json))
        {
            size_t json_class_size = json_array_size(line2_up_json);
            size_t min_class_count = (size_t)system->num_classes < json_class_size ? (size_t)system->num_classes : json_class_size;

            for (size_t class_idx = 0; class_idx < min_class_count; class_idx++)
            {
                json_t *class_up = json_array_get(line2_up_json, class_idx);
                if (json_is_array(class_up))
                {
                    size_t json_lane_size = json_array_size(class_up);
                    size_t min_lane_count = (size_t)system->line2.num_lanes < json_lane_size ? (size_t)system->line2.num_lanes : json_lane_size;

                    for (size_t lane = 0; lane < min_lane_count; lane++)
                    {
                        json_t *count = json_array_get(class_up, lane);
                        if (json_is_integer(count))
                        {
                            int idx = (int)class_idx * system->line2.num_lanes + (int)lane;
                            int value = json_integer_value(count);
                            system->line2.up_counts[idx] = value;
                        }
                    }
                }
            }
        }

        // Get line2 down counts
        json_t *line2_down_json = json_object_get(line2_json, "down_counts");
        if (json_is_array(line2_down_json))
        {
            size_t json_class_size = json_array_size(line2_down_json);
            size_t min_class_count = (size_t)system->num_classes < json_class_size ? (size_t)system->num_classes : json_class_size;

            for (size_t class_idx = 0; class_idx < min_class_count; class_idx++)
            {
                json_t *class_down = json_array_get(line2_down_json, class_idx);
                if (json_is_array(class_down))
                {
                    size_t json_lane_size = json_array_size(class_down);
                    size_t min_lane_count = (size_t)system->line2.num_lanes < json_lane_size ? (size_t)system->line2.num_lanes : json_lane_size;

                    for (size_t lane = 0; lane < min_lane_count; lane++)
                    {
                        json_t *count = json_array_get(class_down, lane);
                        if (json_is_integer(count))
                        {
                            int idx = (int)class_idx * system->line2.num_lanes + (int)lane;
                            int value = json_integer_value(count);
                            system->line2.down_counts[idx] = value;
                        }
                    }
                }
            }
        }
    }

    json_decref(root);
    return true;
}

// Function to save vehicle count data in the requested format
bool save_vehicle_count_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    // Define vehicle types
    const char *vehicle_types[] = {"Car", "Bike", "Truck", "Bus", "Taxi", "Pickup", "Trailer"};
    int num_types = sizeof(vehicle_types) / sizeof(vehicle_types[0]);

    // Create the JSON root object
    json_t *root = json_object();

    // Create arrays for types and quantities
    json_t *type_array = json_array();
    json_t *quantity_array = json_array();

    // Fill in the type array
    for (int i = 0; i < num_types; i++)
    {
        json_array_append_new(type_array, json_string(vehicle_types[i]));
    }

    // Calculate total counts for each type by summing up and down counts across all lanes
    for (int i = 0; i < num_types && i < system->num_classes; i++)
    {
        int total_count = 0;

        // Count from first line
        for (int lane = 0; lane < system->line1.num_lanes; lane++)
        {
            int idx = i * system->line1.num_lanes + lane;
            total_count += system->line1.up_counts[idx] + system->line1.down_counts[idx];
        }

        // Count from second line if enabled
        if (system->use_second_line)
        {
            for (int lane = 0; lane < system->line2.num_lanes; lane++)
            {
                int idx = i * system->line2.num_lanes + lane;
                total_count += system->line2.up_counts[idx] + system->line2.down_counts[idx];
            }
        }

        json_array_append_new(quantity_array, json_integer(total_count));
    }

    // Add arrays to root object
    json_object_set_new(root, "type", type_array);
    json_object_set_new(root, "quantity", quantity_array);

    // Save JSON to file
    int result = json_dump_file(root, filename, JSON_COMPACT | JSON_ENSURE_ASCII);

    json_decref(root);

    if (result == 0)
    {
        // syslog(LOG_INFO, "Vehicle count data successfully saved to %s", filename);
        return true;
    }
    else
    {
        syslog(LOG_ERR, "Failed to save vehicle count data to %s", filename);
        return false;
    }
}

// Function to save vehicle PCU data in the same format as vehicle count data
bool save_vehicle_pcu_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    // Define vehicle types
    const char *vehicle_types[] = {"Car", "Bike", "Truck", "Bus", "Taxi", "Pickup", "Trailer"};
    int num_types = sizeof(vehicle_types) / sizeof(vehicle_types[0]);

    // Create the JSON root object
    json_t *root = json_object();

    // Create arrays for types and PCU values
    json_t *type_array = json_array();
    json_t *quantity_array = json_array();

    // Fill in the type array
    for (int i = 0; i < num_types; i++)
    {
        json_array_append_new(type_array, json_string(vehicle_types[i]));
    }

    // Calculate PCU values for each type by summing up and down counts and applying PCU multipliers
    for (int i = 0; i < num_types && i < system->num_classes; i++)
    {
        int class_count = 0;

        // Count from first line
        for (int lane = 0; lane < system->line1.num_lanes; lane++)
        {
            int idx = i * system->line1.num_lanes + lane;
            class_count += system->line1.up_counts[idx] + system->line1.down_counts[idx];
        }

        // Count from second line if enabled
        if (system->use_second_line)
        {
            for (int lane = 0; lane < system->line2.num_lanes; lane++)
            {
                int idx = i * system->line2.num_lanes + lane;
                class_count += system->line2.up_counts[idx] + system->line2.down_counts[idx];
            }
        }

        // Apply PCU multiplier and add to array
        float pcu_value = class_count * pcu_values[i];
        json_array_append_new(quantity_array, json_real(pcu_value));
    }

    // Add arrays to root object
    json_object_set_new(root, "type", type_array);
    json_object_set_new(root, "quantity", quantity_array);

    // Save JSON to file
    int result = json_dump_file(root, filename, JSON_COMPACT | JSON_ENSURE_ASCII);

    json_decref(root);

    if (result == 0)
    {
        // syslog(LOG_INFO, "Vehicle PCU data successfully saved to %s", filename);
        return true;
    }
    else
    {
        syslog(LOG_ERR, "Failed to save vehicle PCU data to %s", filename);
        return false;
    }
}

bool save_chart_data(const char *filename, int *chart_data, int array_size)
{
    if (!filename || !chart_data || array_size <= 0)
        return false;

    json_t *root = json_object();
    json_t *quantity_json_array = json_array();

    if (!root || !quantity_json_array)
    {
        json_decref(root);
        return false;
    }

    // Add all elements in a single loop
    for (int i = 0; i < array_size; i++)
    {
        json_array_append_new(quantity_json_array, json_integer(chart_data[i]));
    }

    json_object_set_new(root, "type", json_string("Total"));
    json_object_set_new(root, "quantity", quantity_json_array);

    int result = json_dump_file(root, filename, JSON_COMPACT | JSON_ENSURE_ASCII);
    json_decref(root);

    return (result == 0);
}

bool save_chart_data_double(const char *filename, double *chart_data, int array_size)
{
    if (!filename || !chart_data || array_size <= 0)
        return false;

    json_t *root = json_object();
    json_t *quantity_json_array = json_array();

    if (!root || !quantity_json_array)
    {
        json_decref(root);
        return false;
    }

    // Add all elements in a single loop
    for (int i = 0; i < array_size; i++)
    {
        json_array_append_new(quantity_json_array, json_real(chart_data[i]));
    }

    json_object_set_new(root, "type", json_string("Total"));
    json_object_set_new(root, "quantity", quantity_json_array);

    int result = json_dump_file(root, filename, JSON_COMPACT | JSON_ENSURE_ASCII);
    json_decref(root);

    return (result == 0);
}

bool load_chart_data(const char *filename, int *chart_data, int array_size)
{
    if (!filename || !chart_data || array_size <= 0)
        return false;

    // Clear the array before loading
    memset(chart_data, 0, array_size * sizeof(int));

    // Load the JSON file
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root)
    {
        // syslog(LOG_ERR, "Failed to load chart data from %s: %s", filename, error.text);
        return false;
    }

    // Get the quantity array
    json_t *quantity = json_object_get(root, "quantity");

    if (!quantity || !json_is_array(quantity))
    {
        syslog(LOG_ERR, "Invalid format in %s: 'quantity' is not an array", filename);
        json_decref(root);
        return false;
    }

    // Calculate how many elements to process
    size_t json_size = json_array_size(quantity);
    size_t elements_to_read = (json_size < (size_t)array_size) ? json_size : (size_t)array_size;

    // Load values into chart_data - optimize by avoiding excess function calls
    for (size_t i = 0; i < elements_to_read; i++)
    {
        json_t *value = json_array_get(quantity, i);

        if (json_is_integer(value))
        {
            chart_data[i] = json_integer_value(value);
        }
        else if (json_is_real(value))
        {
            // Fix: Avoid casting function call result, use a temporary variable
            double real_value = json_real_value(value);
            chart_data[i] = (int)real_value;
        }
    }

    json_decref(root);
    return true;
}

bool load_chart_data_double(const char *filename, double *chart_data, int array_size)
{
    if (!filename || !chart_data || array_size <= 0)
        return false;

    // Clear the array before loading
    memset(chart_data, 0, array_size * sizeof(double));

    // Load the JSON file
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root)
    {
        // syslog(LOG_ERR, "Failed to load chart data from %s: %s", filename, error.text);
        return false;
    }

    // Get the quantity array
    json_t *quantity = json_object_get(root, "quantity");

    if (!quantity || !json_is_array(quantity))
    {
        syslog(LOG_ERR, "Invalid format in %s: 'quantity' is not an array", filename);
        json_decref(root);
        return false;
    }

    // Calculate how many elements to process
    size_t json_size = json_array_size(quantity);
    size_t elements_to_read = (json_size < (size_t)array_size) ? json_size : (size_t)array_size;

    // Load values into chart_data - optimize by avoiding excess function calls
    for (size_t i = 0; i < elements_to_read; i++)
    {
        json_t *value = json_array_get(quantity, i);

        if (json_is_integer(value))
        {
            // Fix: Avoid casting function call result, use a temporary variable
            json_int_t int_value = json_integer_value(value);
            chart_data[i] = (double)int_value;
        }
        else if (json_is_real(value))
        {
            chart_data[i] = json_real_value(value);
        }
    }

    json_decref(root);
    return true;
}

// Function to calculate the total count across all vehicle types and lanes
int calculate_total_count(CountingSystem *system)
{
    if (!system)
        return 0;

    int total_count = 0;

    for (int i = 0; i < system->num_classes; i++)
    {
        // Count from first line
        for (int lane = 0; lane < system->line1.num_lanes; lane++)
        {
            int idx = i * system->line1.num_lanes + lane;
            total_count += system->line1.up_counts[idx] + system->line1.down_counts[idx];
        }

        // Count from second line if enabled
        if (system->use_second_line)
        {
            for (int lane = 0; lane < system->line2.num_lanes; lane++)
            {
                int idx = i * system->line2.num_lanes + lane;
                total_count += system->line2.up_counts[idx] + system->line2.down_counts[idx];
            }
        }
    }

    return total_count;
}

// Function to calculate the total PCU (Passenger Car Unit) count across all vehicle types and lanes
float calculate_total_pcu(CountingSystem *system)
{
    if (!system)
        return 0.0f;

    float total_pcu = 0.0f;

    // Use the global pcu_values array
    int num_multipliers = NUM_VEHICLE_TYPES;

    for (int i = 0; i < system->num_classes && i < num_multipliers; i++)
    {
        int class_count = 0;

        // Count from first line
        for (int lane = 0; lane < system->line1.num_lanes; lane++)
        {
            int idx = i * system->line1.num_lanes + lane;
            class_count += system->line1.up_counts[idx] + system->line1.down_counts[idx];
        }

        // Count from second line if enabled
        if (system->use_second_line)
        {
            for (int lane = 0; lane < system->line2.num_lanes; lane++)
            {
                int idx = i * system->line2.num_lanes + lane;
                class_count += system->line2.up_counts[idx] + system->line2.down_counts[idx];
            }
        }

        // Apply PCU multiplier for this vehicle class
        total_pcu += class_count * pcu_values[i];
    }

    return total_pcu;
}

bool save_daily_vehicle_count_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    int total_count = calculate_total_count(system);

    if (local_time->tm_hour == 0)
    {
        daily_vehicle_count[local_time->tm_hour] = total_count;
    }
    else
    {
        int pre_total_count = 0;
        for (int i = 0; i < local_time->tm_hour; i++)
        {
            pre_total_count += daily_vehicle_count[i];
        }
        daily_vehicle_count[local_time->tm_hour] = total_count - pre_total_count;
    }

    int array_size = sizeof(daily_vehicle_count) / sizeof(daily_vehicle_count[0]);

    return save_chart_data(filename, daily_vehicle_count, array_size);
}

// Function to save daily PCU data
bool save_daily_vehicle_pcu_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    float total_pcu = calculate_total_pcu(system);

    if (local_time->tm_hour == 0)
    {
        daily_vehicle_pcu[local_time->tm_hour] = total_pcu;
    }
    else
    {
        float pre_total_pcu = 0.0f;
        for (int i = 0; i < local_time->tm_hour; i++)
        {
            pre_total_pcu += daily_vehicle_pcu[i];
        }
        daily_vehicle_pcu[local_time->tm_hour] = total_pcu - pre_total_pcu;
    }

    int array_size = sizeof(daily_vehicle_pcu) / sizeof(daily_vehicle_pcu[0]);

    return save_chart_data_double(filename, daily_vehicle_pcu, array_size);
}

bool save_weekly_vehicle_count_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    int total_count = calculate_total_count(system);

    weekly_vehicle_count[WEEKLY_ARRAY_SIZE - 1] = total_count;

    int array_size = sizeof(weekly_vehicle_count) / sizeof(weekly_vehicle_count[0]);

    return save_chart_data(filename, weekly_vehicle_count, array_size);
}

bool save_weekly_vehicle_pcu_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    float total_pcu = calculate_total_pcu(system);

    weekly_vehicle_pcu[WEEKLY_ARRAY_SIZE - 1] = total_pcu;

    int array_size = sizeof(weekly_vehicle_pcu) / sizeof(weekly_vehicle_pcu[0]);

    return save_chart_data_double(filename, weekly_vehicle_pcu, array_size);
}

bool save_average_speed_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    average_speed[0] = get_average_velocity(system, 3600000, -1);

    int array_size = sizeof(average_speed) / sizeof(average_speed[0]);

    return save_chart_data_double(filename, average_speed, array_size);
}

bool save_daily_average_speed_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    daily_average_speed[local_time->tm_hour] = get_average_velocity(system, 3600000, -1);

    int array_size = sizeof(daily_average_speed) / sizeof(daily_average_speed[0]);

    return save_chart_data_double(filename, daily_average_speed, array_size);
}

bool save_weekly_average_speed_data(CountingSystem *system, const char *filename)
{
    if (!system || !filename)
        return false;

    double sum = 0;
    int count = 0;

    for (int i = 0; i < DAILY_ARRAY_SIZE; i++)
    {
        if (fabs(daily_average_speed[i]) > 0.0001)
        {
            sum += daily_average_speed[i];
            count++;
        }
    }

    // syslog(LOG_INFO, "average: %f / %d = %f", sum, count,  sum / count);

    if (!count)
        return false;

    weekly_average_speed[WEEKLY_ARRAY_SIZE - 1] = sum / count;

    int array_size = sizeof(weekly_average_speed) / sizeof(weekly_average_speed[0]);

    return save_chart_data_double(filename, weekly_average_speed, array_size);
}

void shift_array_left(int array[], int array_size)
{
    if (!array || array_size <= 1)
        return;

    // Use memmove for efficient memory movement
    memmove(array, array + 1, (array_size - 1) * sizeof(int));
    array[array_size - 1] = 0;
}

void shift_array_left_double(double array[], int array_size)
{
    if (!array || array_size <= 1)
        return;

    // Use memmove for efficient memory movement
    memmove(array, array + 1, (array_size - 1) * sizeof(double));
    array[array_size - 1] = 0.0;
}