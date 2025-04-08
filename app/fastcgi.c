#include "fastcgi.h"
#include "incident.h"

#include "uriparser/Uri.h"
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#define FCGI_SOCKET_NAME "FCGI_SOCKET_NAME"
#define MAX_CONTENT_LENGTH 1024 * 1024 // 1MB max content length
#define JSON_FLAGS (JSON_COMPACT | JSON_ENSURE_ASCII)
#define MAX_FILENAME_LENGTH 256
#define STORAGE_PATH "/usr/local/packages/enixma_analytic/localdata"

// Thread for handling FastCGI requests
pthread_t fcgi_thread;
volatile int fcgi_running = 1;

double confidence = 50.0;
double pixels_per_meter = 50.0;
bool first_wrongway = false;
bool second_wrongway = false;
bool first_truckright = false;
bool second_truckright = false;
int first_overspeed = 0;
int second_overspeed = 0;

// Global variables for tracking incident reception status
IncidentData first_incidents = {0};
IncidentData second_incidents = {0};

// Global variables for tracking limit speed reception status
LimitSpeedData first_limitspeed = {0};
LimitSpeedData second_limitspeed = {0};

bool first_incidents_received = false;
bool second_incidents_received = false;

bool first_overspeed_received = false;
bool second_overspeed_received = false;

bool first_limitspeed_received = false;
bool second_limitspeed_received = false;

// Default PCU values for different vehicle types
float pcu_values[NUM_VEHICLE_TYPES] = {
    1.0f,  // Car
    0.25f, // Bike
    2.5f,  // Truck
    2.0f,  // Bus
    1.0f,  // Taxi
    1.0f,  // Pickup
    2.5f   // Trailer
};

void process_polygon(Polygon **roi, json_t *json_data)
{
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");

    if (!data_array || !json_is_array(data_array))
    {
        return;
    }

    *roi = init_polygon(MAX_POLYGON_POINTS);

    if (!*roi)
    {
        printf("Failed to initialize ROIs\n");
        return;
    }

    size_t index;
    json_t *coord;
    json_array_foreach(data_array, index, coord)
    {
        json_t *x = json_object_get(coord, "x");
        json_t *y = json_object_get(coord, "y");

        if (json_is_number(x) && json_is_number(y))
        {
            add_polygon_point(*roi,
                              json_number_value(x) / context.resolution.widthFrameHD,
                              json_number_value(y) / context.resolution.heightFrameHD);
        }
    }
}

// Process crossline data from JSON, supporting both 2-point and multi-point formats
// Now handles {"points":[{"x":80,"y":407},{"x":437,"y":407}],"direction":true} format
MultiLineCoordinates process_crossline(json_t *json_data)
{
    MultiLineCoordinates coords;
    coords.num_points = 0;
    memset(coords.points, 0, sizeof(coords.points));

    // Check for the new format with "points" array
    json_t *data_array = json_object_get(json_data, "points");

    // If "points" is not found, fall back to the old format checking
    if (!data_array || !json_is_array(data_array))
    {
        // Try original format (either direct array or "data" object)
        data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");

        if (!data_array || !json_is_array(data_array))
        {
            return coords;
        }
    }

    // Count the number of points (limited by MAX_SEGMENTS)
    size_t num_points = json_array_size(data_array);
    if (num_points > MAX_SEGMENTS)
    {
        num_points = MAX_SEGMENTS;
    }

    if (num_points < 2)
    {
        // Need at least 2 points for a valid line
        return coords;
    }

    coords.num_points = num_points;

    // Process each point
    for (size_t i = 0; i < num_points; i++)
    {
        json_t *point = json_array_get(data_array, i);
        json_t *x_json = json_object_get(point, "x");
        json_t *y_json = json_object_get(point, "y");

        if (!x_json || !y_json)
        {
            // If any point is invalid, reset and return empty
            coords.num_points = 0;
            return coords;
        }

        // Get the numeric values and normalize
        double x = json_number_value(x_json);
        double y = json_number_value(y_json);

        // Normalize coordinates
        coords.points[i].x = x / context.resolution.widthFrameHD;
        coords.points[i].y = y / context.resolution.heightFrameHD;
    }

    return coords;
}

// Update the set_crossline_values function to check for empty points array
void set_crossline_values(const char *name_param, json_t *json_data)
{
    if (!name_param || !counting_system)
        return;

    // Process the crossline with multi-point support
    MultiLineCoordinates multi_coords = process_crossline(json_data);

    // Check if this is an empty array (for deletion)
    json_t *points_array = json_object_get(json_data, "points");
    json_t *data_array = NULL;

    if (points_array && json_is_array(points_array))
    {
        data_array = points_array;
    }
    else
    {
        data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");
    }

    bool is_empty_array = data_array && json_is_array(data_array) && json_array_size(data_array) == 0;

    if (is_empty_array)
    {
        // Handle as deletion - reset the line
        if (strcmp(name_param, "firstCrossline") == 0)
        {
            // Reset first line to a default state (single lane with no points)
            resize_line_lanes(counting_system, LINE_1, 1);
            // Reset the points to zeros
            LinePoint default_points[2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
            update_line_points(counting_system, LINE_1, default_points, 2);
        }
        else if (strcmp(name_param, "secondCrossline") == 0)
        {
            // Reset second line
            if (counting_system->use_second_line)
            {
                resize_line_lanes(counting_system, LINE_2, 1);
                LinePoint default_points[2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
                update_line_points(counting_system, LINE_2, default_points, 2);
            }
        }
        return;
    }

    // Normal processing for non-empty arrays
    if (multi_coords.num_points < 2)
        return;

    // Extract lane count (points - 1)
    int num_lanes = multi_coords.num_points - 1;

    if (strcmp(name_param, "firstCrossline") == 0)
    {
        // For the first line
        if (resize_line_lanes(counting_system, LINE_1, num_lanes))
        {
            update_line_points(counting_system, LINE_1, multi_coords.points, multi_coords.num_points);
        }
    }
    else if (strcmp(name_param, "secondCrossline") == 0)
    {
        // For the second line
        if (resize_line_lanes(counting_system, LINE_2, num_lanes))
        {
            update_line_points(counting_system, LINE_2, multi_coords.points, multi_coords.num_points);
        }
    }

    // Store direction information if available
    json_t *direction_json = json_object_get(json_data, "direction");
    if (json_is_boolean(direction_json))
    {
        bool direction = json_boolean_value(direction_json);
        // Set direction based on the line type
        if (strcmp(name_param, "firstCrossline") == 0)
        {
            counting_system->line1_direction = direction;
        }
        else if (strcmp(name_param, "secondCrossline") == 0)
        {
            counting_system->line2_direction = direction;
        }
    }
}

// Function to process silder data from JSON
double process_slider(json_t *json_data)
{
    if (!json_data)
    {
        return 0.0;
    }

    // Get data array (check if input is data array directly or under "data" key)
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");
    if (!data_array || !json_is_array(data_array))
    {
        return 0.0;
    }

    // Get first object
    json_t *object = json_array_get(data_array, 0);
    if (!object)
    {
        return 0.0;
    }

    // Get value field
    json_t *value = json_object_get(object, "value");
    if (!value || !json_is_number(value))
    {
        return 0.0;
    }

    return json_number_value(value);
}

// Function to process toggle data from JSON
bool process_toggle(json_t *json_data)
{
    if (!json_data)
    {
        return false;
    }

    // Get array (check if input is array directly or under "data" key)
    json_t *array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");
    if (!array || !json_is_array(array))
    {
        return false;
    }

    // Get first object in the array
    json_t *object = json_array_get(array, 0);
    if (!object)
    {
        return false;
    }

    // Get value field
    json_t *value = json_object_get(object, "value");
    if (!value || !json_is_boolean(value))
    {
        return false;
    }

    return json_boolean_value(value);
}

// Function to process incident data from JSON
IncidentData process_incidents(json_t *json_data, bool *received_flag)
{
    IncidentData incident = {0}; // Initialize all fields to 0/false

    // Set received_flag to false by default (if provided)
    if (received_flag != NULL)
    {
        *received_flag = false;
    }

    if (!json_data)
    {
        return incident;
    }

    // Get array (check if input is array directly or under "data" key)
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");

    // Check if the array exists and is an array
    if (!data_array || !json_is_array(data_array))
    {
        return incident;
    }

    // Check array length
    size_t array_size = json_array_size(data_array);

    // Set received_flag based on array length
    if (array_size > 0 && received_flag != NULL)
    {
        *received_flag = true;
    }

    // If array has any elements, extract the data
    if (array_size > 0)
    {
        // Get first object in the array
        json_t *object = json_array_get(data_array, 0);
        if (!object)
        {
            return incident;
        }

        // Extract timer value
        json_t *timer_json = json_object_get(object, "timer");
        if (json_is_integer(timer_json))
        {
            incident.timer = json_integer_value(timer_json);
        }

        // Extract boolean incident flags
        json_t *accident_json = json_object_get(object, "accident");
        if (json_is_boolean(accident_json))
        {
            incident.accident = json_boolean_value(accident_json);
        }

        json_t *broken_json = json_object_get(object, "broken");
        if (json_is_boolean(broken_json))
        {
            incident.broken = json_boolean_value(broken_json);
        }

        json_t *stop_json = json_object_get(object, "stop");
        if (json_is_boolean(stop_json))
        {
            incident.stop = json_boolean_value(stop_json);
        }

        json_t *block_json = json_object_get(object, "block");
        if (json_is_boolean(block_json))
        {
            incident.block = json_boolean_value(block_json);
        }

        json_t *construction_json = json_object_get(object, "construction");
        if (json_is_boolean(construction_json))
        {
            incident.construction = json_boolean_value(construction_json);
        }
    }

    return incident;
}

int process_overspeed(json_t *json_data, bool *received_flag)
{
    // Set received_flag to false by default (if provided)
    if (received_flag != NULL)
    {
        *received_flag = false;
    }

    if (!json_data)
    {
        return 0;
    }

    // Get data array (check if input is data array directly or under "data" key)
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");
    if (!data_array || !json_is_array(data_array))
    {
        return 0;
    }

    // Check array length
    size_t array_size = json_array_size(data_array);

    // Set received_flag based on array length
    if (array_size > 0 && received_flag != NULL)
    {
        *received_flag = true;
    }

    // If array has no elements, return 0
    if (array_size == 0)
    {
        return 0;
    }

    // Get first object
    json_t *object = json_array_get(data_array, 0);
    if (!object || !json_is_object(object))
    {
        return 0;
    }

    // Get value field
    json_t *value = json_object_get(object, "value");
    if (!value || !json_integer_value(value))
    {
        return 0;
    }

    return json_integer_value(value);
}

// Function to process limitspeed data from JSON
LimitSpeedData process_limitspeed(json_t *json_data, bool *received_flag)
{
    LimitSpeedData limitspeed = {0}; // Initialize all fields to 0/false

    // Set received_flag to false by default (if provided)
    if (received_flag != NULL)
    {
        *received_flag = false;
    }

    if (!json_data)
    {
        return limitspeed;
    }

    // Get array (check if input is array directly or under "data" key)
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");

    // Check if the array exists and is an array
    if (!data_array || !json_is_array(data_array))
    {
        return limitspeed;
    }

    // Check array length
    size_t array_size = json_array_size(data_array);

    // Set received_flag based on array length
    if (array_size > 0 && received_flag != NULL)
    {
        *received_flag = true;
    }

    // If array has any elements, extract the data
    if (array_size > 0)
    {
        // Get first object in the array
        json_t *object = json_array_get(data_array, 0);
        if (!object)
        {
            return limitspeed;
        }

        // Extract limit speed value
        json_t *min_json = json_object_get(object, "min");
        if (json_is_integer(min_json))
        {
            limitspeed.min = json_integer_value(min_json);
        }

        json_t *max_json = json_object_get(object, "max");
        if (json_is_integer(max_json))
        {
            limitspeed.max = json_integer_value(max_json);
        }
    }

    return limitspeed;
}

// Function to process PCU data from JSON
void process_pcu(json_t *json_data, float *values)
{
    if (!json_data || !values)
    {
        return;
    }

    // // Set default values
    // values[0] = 1.0f;      // Car
    // values[1] = 0.25f;     // Bike
    // values[2] = 2.5f;      // Truck
    // values[3] = 2.0f;      // Bus
    // values[4] = 1.0f;      // Taxi
    // values[5] = 1.0f;      // Pickup
    // values[6] = 2.5f;      // Trailer

    // Get array (check if input is array directly or under "data" key)
    json_t *data_array = json_is_array(json_data) ? json_data : json_object_get(json_data, "data");
    if (!data_array || !json_is_array(data_array) || json_array_size(data_array) == 0)
    {
        return;
    }

    // Get first object in the array
    json_t *object = json_array_get(data_array, 0);
    if (!object)
    {
        return;
    }

    // Extract PCU values if they exist
    json_t *car_json = json_object_get(object, "car_pcu");
    if (json_is_number(car_json))
    {
        values[0] = (float)json_number_value(car_json);
    }

    json_t *bike_json = json_object_get(object, "bike_pcu");
    if (json_is_number(bike_json))
    {
        values[1] = (float)json_number_value(bike_json);
    }

    json_t *truck_json = json_object_get(object, "truck_pcu");
    if (json_is_number(truck_json))
    {
        values[2] = (float)json_number_value(truck_json);
    }

    json_t *bus_json = json_object_get(object, "bus_pcu");
    if (json_is_number(bus_json))
    {
        values[3] = (float)json_number_value(bus_json);
    }

    json_t *taxi_json = json_object_get(object, "taxi_pcu");
    if (json_is_number(taxi_json))
    {
        values[4] = (float)json_number_value(taxi_json);
    }

    json_t *pickup_json = json_object_get(object, "pickup_pcu");
    if (json_is_number(pickup_json))
    {
        values[5] = (float)json_number_value(pickup_json);
    }

    json_t *trailer_json = json_object_get(object, "trailer_pcu");
    if (json_is_number(trailer_json))
    {
        values[6] = (float)json_number_value(trailer_json);
    }
}

// Function to set values based on name type
void set_name_values(const char *name_param, json_t *json_data)
{
    if (!name_param)
    {
        return;
    }

    if (strcmp(name_param, "firstPoly") == 0)
    {
        process_polygon(&roi1, json_data);
    }
    else if (strcmp(name_param, "secondPoly") == 0)
    {
        process_polygon(&roi2, json_data);
    }
    else if (strcmp(name_param, "firstCrossline") == 0 || strcmp(name_param, "secondCrossline") == 0)
    {
        set_crossline_values(name_param, json_data);
    }
    else if (strcmp(name_param, "confidence") == 0)
    {
        confidence = process_slider(json_data);
    }
    else if (strcmp(name_param, "ppm") == 0)
    {
        pixels_per_meter = process_slider(json_data);
    }
    else if (strcmp(name_param, "firstWrongWay") == 0)
    {
        first_wrongway = process_toggle(json_data);
    }
    else if (strcmp(name_param, "secondWrongWay") == 0)
    {
        second_wrongway = process_toggle(json_data);
    }
    else if (strcmp(name_param, "firstIncidents") == 0)
    {
        first_incidents = process_incidents(json_data, &first_incidents_received);

        // Update ROI1 event settings
        ROIEventSettings settings;
        settings.enabled = first_incidents_received;
        settings.timer = first_incidents.timer;
        settings.accident = first_incidents.accident;
        settings.broken = first_incidents.broken;
        settings.stop = first_incidents.stop;
        settings.block = first_incidents.block;
        settings.construction = first_incidents.construction;

        // Update the ROI1 event settings
        update_roi_event_settings(1, settings);
    }
    else if (strcmp(name_param, "secondIncidents") == 0)
    {
        second_incidents = process_incidents(json_data, &second_incidents_received);

        // Update ROI2 event settings
        ROIEventSettings settings;
        settings.enabled = second_incidents_received;
        settings.timer = second_incidents.timer > 0 ? second_incidents.timer : 30;
        settings.accident = second_incidents.accident;
        settings.broken = second_incidents.broken;
        settings.stop = second_incidents.stop;
        settings.block = second_incidents.block;
        settings.construction = second_incidents.construction;

        // Update the ROI2 event settings
        update_roi_event_settings(2, settings);
    }
    else if (strcmp(name_param, "firstTruckRight") == 0)
    {
        first_truckright = process_toggle(json_data);
        // syslog(LOG_INFO, "firstTruckRight: %d", first_truckright);
    }
    else if (strcmp(name_param, "secondTruckRight") == 0)
    {
        second_truckright = process_toggle(json_data);
        // syslog(LOG_INFO, "secondTruckRight: %d", second_truckright);
    }
    else if (strcmp(name_param, "firstOverSpeed") == 0)
    {
        first_overspeed = process_overspeed(json_data, &first_overspeed_received);
        // syslog(LOG_INFO, "firstOverSpeed: %d, Received Flag: %d", first_overspeed, first_overspeed_received);
    }
    else if (strcmp(name_param, "secondOverSpeed") == 0)
    {
        second_overspeed = process_overspeed(json_data, &second_overspeed_received);
        // syslog(LOG_INFO, "secondOverSpeed: %d, Received Flag: %d", second_overspeed, second_overspeed_received);
    }
    else if (strcmp(name_param, "firstLimitSpeed") == 0)
    {
        first_limitspeed = process_limitspeed(json_data, &first_limitspeed_received);
        // syslog(LOG_INFO, "firstLimitSpeed-Min: %d, firstLimitSpeed-Max: %d, Received Flag: %d", first_limitspeed.min, first_limitspeed.max, first_limitspeed_received);
    }
    else if (strcmp(name_param, "secondLimitSpeed") == 0)
    {
        second_limitspeed = process_limitspeed(json_data, &second_limitspeed_received);
        // syslog(LOG_INFO, "secondLimitSpeed-Min: %d, firstLimitSpeed-Max: %d, Received Flag: %d", second_limitspeed.min, second_limitspeed.max, second_limitspeed_received);
    }
    else if (strcmp(name_param, "pcu") == 0)
    {
        process_pcu(json_data, pcu_values);

        // syslog(LOG_INFO, "PCU values updated - Car: %.2f, Bike: %.2f, Truck: %.2f, Bus: %.2f, Taxi: %.2f, Pickup: %.2f, Trailer: %.2f",
        //        pcu_values[0], pcu_values[1], pcu_values[2],
        //        pcu_values[3], pcu_values[4], pcu_values[5],
        //        pcu_values[6]);
    }
}

// Function to ensure storage directory exists
int ensure_storage_directory(void)
{
    struct stat st = {0};
    if (stat(STORAGE_PATH, &st) == -1)
    {
        // Create directory with full permissions
        if (mkdir(STORAGE_PATH, 0777) == -1)
        {
            syslog(LOG_ERR, "Failed to create storage directory: %s", strerror(errno));
            return -1;
        }
        // Set directory permissions
        if (chmod(STORAGE_PATH, 0777) == -1)
        {
            syslog(LOG_ERR, "Failed to set directory permissions: %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

// Function to create a filename from name parameter
char *create_filename(const char *name_param)
{
    if (!name_param)
        return NULL;

    char *filename = malloc(MAX_FILENAME_LENGTH);
    if (!filename)
        return NULL;

    // Create full path including directory
    snprintf(filename, MAX_FILENAME_LENGTH, "%s/%s.json", STORAGE_PATH, name_param);
    return filename;
}

// Function to save JSON data to file
int save_to_file(const char *name_param, json_t *data)
{
    if (!name_param || !data)
        return -1;

    char *filename = create_filename(name_param);
    if (!filename)
        return -1;

    int result = -1;
    char *json_str = json_dumps(data, JSON_FLAGS);

    if (json_str)
    {
        FILE *file = fopen(filename, "w");
        if (file)
        {
            if (fputs(json_str, file) >= 0)
            {
                result = 0; // Success
            }
            fclose(file);
        }
        free(json_str);
    }

    free(filename);
    return result;
}

// Function to load JSON data from file
json_t *load_from_file(const char *name_param)
{
    if (!name_param)
        return NULL;

    char *filename = create_filename(name_param);
    if (!filename)
        return NULL;

    json_t *data = NULL;
    json_error_t error;

    FILE *file = fopen(filename, "r");
    if (file)
    {
        // Get file size
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (size > 0)
        {
            char *buffer = malloc(size + 1);
            if (buffer)
            {
                if (fread(buffer, 1, size, file) == (size_t)size)
                {
                    buffer[size] = '\0';
                    data = json_loads(buffer, 0, &error);
                }
                free(buffer);
            }
        }
        fclose(file);
    }

    free(filename);
    return data;
}

char *get_file_contents(const char *path)
{
    if (!path)
    {
        syslog(LOG_ERR, "Invalid path parameter");
        return NULL;
    }

    // Get the filename from the full path
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path; // If no slash found, use the whole path

    char *file_contents = NULL;
    FILE *file = fopen(path, "rb");

    if (!file)
    {
        // syslog(LOG_INFO, "unable to 'open' %s", filename);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        syslog(LOG_ERR, "Failed to seek in file %s", filename);
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size == -1)
    {
        syslog(LOG_ERR, "Failed to determine file size for %s", filename);
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        syslog(LOG_ERR, "Failed to seek back to start of file %s", filename);
        fclose(file);
        return NULL;
    }

    file_contents = (char *)malloc(file_size + 1);
    if (!file_contents)
    {
        syslog(LOG_ERR, "Memory allocation failed for %s (%ld bytes)", filename, file_size + 1);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(file_contents, 1, file_size, file);
    if (bytes_read != (size_t)file_size)
    {
        syslog(LOG_ERR, "Failed to read entire file %s", filename);
        free(file_contents);
        fclose(file);
        return NULL;
    }

    file_contents[file_size] = '\0';
    fclose(file);
    // syslog(LOG_INFO, "%s was 'read'", filename);

    return file_contents;
}

void get_parameters(void)
{
    // Process roi1
    char *roi1_filename = create_filename("firstPoly");
    if (roi1_filename)
    {
        char *roi1_content = get_file_contents(roi1_filename);
        free(roi1_filename);

        if (roi1_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(roi1_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                process_polygon(&roi1, json_array);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for roi1: %s", error.text);
            }
            free(roi1_content);
        }
    }

    // Process roi2
    char *roi2_filename = create_filename("secondPoly");
    if (roi2_filename)
    {
        char *roi2_content = get_file_contents(roi2_filename);
        free(roi2_filename);

        if (roi2_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(roi2_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                process_polygon(&roi2, json_array);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for roi2: %s", error.text);
            }
            free(roi2_content);
        }
    }

    // Process crossline1
    char *crossline1_filename = create_filename("firstCrossline");
    if (crossline1_filename)
    {
        char *crossline1_content = get_file_contents(crossline1_filename);
        free(crossline1_filename);

        if (crossline1_content)
        {
            json_error_t error;
            json_t *json_obj = json_loads(crossline1_content, 0, &error);
            if (json_obj)
            {
                set_crossline_values("firstCrossline", json_obj);
                json_decref(json_obj);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for crossline1: %s", error.text);
            }
            free(crossline1_content);
        }
    }

    // Process crossline2
    char *crossline2_filename = create_filename("secondCrossline");
    if (crossline2_filename)
    {
        char *crossline2_content = get_file_contents(crossline2_filename);
        free(crossline2_filename);

        if (crossline2_content)
        {
            json_error_t error;
            json_t *json_obj = json_loads(crossline2_content, 0, &error);
            if (json_obj)
            {
                set_crossline_values("secondCrossline", json_obj);
                json_decref(json_obj);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for crossline2: %s", error.text);
            }
            free(crossline2_content);
        }
    }

    // Process confidence
    char *confidence_filename = create_filename("confidence");
    if (confidence_filename)
    {
        char *confidence_content = get_file_contents(confidence_filename);
        free(confidence_filename);

        if (confidence_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(confidence_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                confidence = process_slider(json_data);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for confidence: %s", error.text);
            }
            free(confidence_content);
        }
    }

    // Process ppm
    char *ppm_filename = create_filename("ppm");
    if (ppm_filename)
    {
        char *ppm_content = get_file_contents(ppm_filename);
        free(ppm_filename);

        if (ppm_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(ppm_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                pixels_per_meter = process_slider(json_data);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for ppm: %s", error.text);
            }
            free(ppm_content);
        }
    }

    // Process first wrongway toggle
    char *first_wrongway_filename = create_filename("firstWrongWay");
    if (first_wrongway_filename)
    {
        char *wrongway_content = get_file_contents(first_wrongway_filename);
        free(first_wrongway_filename);

        if (wrongway_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(wrongway_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                first_wrongway = process_toggle(json_data);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for firstWrongWay: %s", error.text);
            }
            free(wrongway_content);
        }
    }

    // Process second wrongway toggle
    char *second_wrongway_filename = create_filename("secondWrongWay");
    if (second_wrongway_filename)
    {
        char *wrongway_content = get_file_contents(second_wrongway_filename);
        free(second_wrongway_filename);

        if (wrongway_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(wrongway_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                second_wrongway = process_toggle(json_data);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for secondWrongWay: %s", error.text);
            }
            free(wrongway_content);
        }
    }

    // Process first incidents
    char *first_incidents_filename = create_filename("firstIncidents");
    if (first_incidents_filename)
    {
        char *incidents_content = get_file_contents(first_incidents_filename);
        free(first_incidents_filename);

        if (incidents_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(incidents_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                first_incidents = process_incidents(json_data, &first_incidents_received);

                // Update ROI1 event settings
                ROIEventSettings settings;
                settings.enabled = first_incidents_received;
                settings.timer = first_incidents.timer;
                settings.accident = first_incidents.accident;
                settings.broken = first_incidents.broken;
                settings.stop = first_incidents.stop;
                settings.block = first_incidents.block;
                settings.construction = first_incidents.construction;

                // Update the ROI1 event settings
                update_roi_event_settings(1, settings);

                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for firstIncidents: %s", error.text);
            }
            free(incidents_content);
        }
    }

    // Process second incidents
    char *second_incidents_filename = create_filename("secondIncidents");
    if (second_incidents_filename)
    {
        char *incidents_content = get_file_contents(second_incidents_filename);
        free(second_incidents_filename);

        if (incidents_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(incidents_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                second_incidents = process_incidents(json_data, &second_incidents_received);

                // Update ROI2 event settings
                ROIEventSettings settings;
                settings.enabled = second_incidents_received;
                settings.timer = second_incidents.timer > 0 ? second_incidents.timer : 30;
                settings.accident = second_incidents.accident;
                settings.broken = second_incidents.broken;
                settings.stop = second_incidents.stop;
                settings.block = second_incidents.block;
                settings.construction = second_incidents.construction;

                // Update the ROI2 event settings
                update_roi_event_settings(2, settings);

                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for secondIncidents: %s", error.text);
            }
            free(incidents_content);
        }
    }

    // Process first truckright toggle
    char *first_truckright_filename = create_filename("firstTruckRight");
    if (first_truckright_filename)
    {
        char *truckright_content = get_file_contents(first_truckright_filename);
        free(first_truckright_filename);

        if (truckright_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(truckright_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                first_truckright = process_toggle(json_data);
                // syslog(LOG_INFO, "firstTruckRight: %d", first_truckright);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for firstTruckRight: %s", error.text);
            }
            free(truckright_content);
        }
    }

    // Process second truckright toggle
    char *second_truckright_filename = create_filename("secondTruckRight");
    if (second_truckright_filename)
    {
        char *truckright_content = get_file_contents(second_truckright_filename);
        free(second_truckright_filename);

        if (truckright_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(truckright_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                second_truckright = process_toggle(json_data);
                // syslog(LOG_INFO, "secondTruckRight: %d", second_truckright);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for secondTruckRight: %s", error.text);
            }
            free(truckright_content);
        }
    }

    // Process first overspeed toggle
    char *first_overspeed_filename = create_filename("firstOverSpeed");
    if (first_overspeed_filename)
    {
        char *overspeed_content = get_file_contents(first_overspeed_filename);
        free(first_overspeed_filename);

        if (overspeed_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(overspeed_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                first_overspeed = process_overspeed(json_data, &first_overspeed_received);
                // syslog(LOG_INFO, "firstOverSpeed: %d, Received Flag: %d", first_overspeed, first_overspeed_received);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for firstOverSpeed: %s", error.text);
            }
            free(overspeed_content);
        }
    }

    // Process second overspeed toggle
    char *second_overspeed_filename = create_filename("secondOverSpeed");
    if (second_overspeed_filename)
    {
        char *overspeed_content = get_file_contents(second_overspeed_filename);
        free(second_overspeed_filename);

        if (overspeed_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(overspeed_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                second_overspeed = process_overspeed(json_data, &second_overspeed_received);
                // syslog(LOG_INFO, "secondOverSpeed: %d, Received Flag: %d", second_overspeed, second_overspeed_received);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for secondOverSpeed: %s", error.text);
            }
            free(overspeed_content);
        }
    }

    // Process first limitspeed toggle
    char *first_limitspeed_filename = create_filename("firstLimitSpeed");
    if (first_limitspeed_filename)
    {
        char *limitspeed_content = get_file_contents(first_limitspeed_filename);
        free(first_limitspeed_filename);

        if (limitspeed_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(limitspeed_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                first_limitspeed = process_limitspeed(json_data, &first_limitspeed_received);
                // syslog(LOG_INFO, "firstLimitSpeed-Min: %d, firstLimitSpeed-Max: %d, Received Flag: %d", first_limitspeed.min, first_limitspeed.max, first_limitspeed_received);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for firstLimitSpeed: %s", error.text);
            }
            free(limitspeed_content);
        }
    }

    // Process second limitspeed toggle
    char *second_limitspeed_filename = create_filename("secondLimitSpeed");
    if (second_limitspeed_filename)
    {
        char *limitspeed_content = get_file_contents(second_limitspeed_filename);
        free(second_limitspeed_filename);

        if (limitspeed_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(limitspeed_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                second_limitspeed = process_limitspeed(json_data, &second_limitspeed_received);
                // syslog(LOG_INFO, "secondLimitSpeed-Min: %d, firstLimitSpeed-Max: %d, Received Flag: %d", second_limitspeed.min, second_limitspeed.max, second_limitspeed_received);
                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for secondLimitSpeed: %s", error.text);
            }
            free(limitspeed_content);
        }
    }

    // Process PCU values
    char *pcu_filename = create_filename("pcu");
    if (pcu_filename)
    {
        char *pcu_content = get_file_contents(pcu_filename);
        free(pcu_filename);

        if (pcu_content)
        {
            json_error_t error;
            json_t *json_array = json_loads(pcu_content, 0, &error);
            if (json_array)
            {
                json_t *json_data = json_object();
                json_object_set_new(json_data, "data", json_array);

                process_pcu(json_data, pcu_values);

                // syslog(LOG_INFO, "PCU values loaded from file - Car: %.2f, Bike: %.2f, Truck: %.2f, Bus: %.2f, Taxi: %.2f, Pickup: %.2f, Trailer: %.2f",
                //        pcu_values[0], pcu_values[1], pcu_values[2],
                //        pcu_values[3], pcu_values[4], pcu_values[5],
                //        pcu_values[6]);

                json_decref(json_data);
            }
            else
            {
                syslog(LOG_ERR, "JSON parsing failed for pcu: %s", error.text);
            }
            free(pcu_content);
        }
    }
}

// Function to send JSON response
void send_json_response(FCGX_Stream *out, json_t *json)
{
    if (!json)
    {
        FCGX_FPrintF(out, "Content-Type: application/json\r\n\r\n");
        FCGX_FPrintF(out, "{\"error\":\"Invalid JSON object\"}");
        return;
    }

    char *json_str = json_dumps(json, JSON_FLAGS);
    if (json_str)
    {
        FCGX_FPrintF(out, "Content-Type: application/json\r\n");
        FCGX_FPrintF(out, "Content-Length: %d\r\n", strlen(json_str));
        FCGX_FPrintF(out, "\r\n");
        FCGX_FPrintF(out, "%s", json_str);
        free(json_str);
    }
    else
    {
        FCGX_FPrintF(out, "Content-Type: application/json\r\n\r\n");
        FCGX_FPrintF(out, "{\"error\":\"Failed to generate JSON response\"}");
    }
}

// Function to parse query parameters
json_t *parse_query_params(const char *query)
{
    json_t *params = json_object();
    if (!params || !query)
    {
        return params;
    }

    char *query_copy = strdup(query);
    char *pair, *saveptr1, *saveptr2;

    pair = strtok_r(query_copy, "&", &saveptr1);
    while (pair)
    {
        char *key = strtok_r(pair, "=", &saveptr2);
        char *value = strtok_r(NULL, "=", &saveptr2);

        if (key && value)
        {
            // URL decode the value
            char *decoded_value = malloc(strlen(value) + 1);
            if (decoded_value)
            {
                char *src = value;
                char *dst = decoded_value;
                while (*src)
                {
                    if (*src == '%' && src[1] && src[2])
                    {
                        int hex;
                        sscanf(src + 1, "%2x", &hex);
                        *dst++ = hex;
                        src += 3;
                    }
                    else if (*src == '+')
                    {
                        *dst++ = ' ';
                        src++;
                    }
                    else
                    {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';
                json_object_set_new(params, key, json_string(decoded_value));
                free(decoded_value);
            }
        }
        pair = strtok_r(NULL, "&", &saveptr1);
    }

    free(query_copy);
    return params;
}

// Function to get name parameter from query parameters
const char *get_name_param(json_t *query_params)
{
    if (!query_params)
        return NULL;
    json_t *name = json_object_get(query_params, "name");
    return name ? json_string_value(name) : NULL;
}

// Function to get all files data
json_t *get_all_files_data(void)
{
    json_t *all_data = json_object();
    DIR *dir;
    struct dirent *entry;

    dir = opendir(STORAGE_PATH);
    if (!dir)
    {
        syslog(LOG_ERR, "Failed to open storage directory: %s", strerror(errno));
        return all_data;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and .. directories and non-.json files
        // Check if file ends with .json using string comparison
        if (strstr(entry->d_name, ".json"))
        {
            // Extract name without .json extension
            char *name = strdup(entry->d_name);
            char *dot = strrchr(name, '.');
            if (dot)
                *dot = '\0';

            // Load data for this file
            json_t *file_data = load_from_file(name);
            if (file_data)
            {
                json_object_set_new(all_data, name, file_data);
            }

            free(name);
        }
    }

    closedir(dir);
    return all_data;
}

// Function to handle GET request
void handle_get_request(FCGX_Stream *out, json_t *query_params)
{
    json_t *response = json_object();
    const char *name_param = get_name_param(query_params);

    json_object_set_new(response, "method", json_string("GET"));

    if (name_param)
    {
        // Handle specific file request
        json_t *data = load_from_file(name_param);
        if (data)
        {
            json_object_set_new(response, "data", data);
        }
        else
        {
            json_object_set_new(response, "data", json_null());
        }
        json_object_set_new(response, "name", json_string(name_param));
    }
    else
    {
        // Handle request for all files
        json_t *all_data = get_all_files_data();
        json_object_set_new(response, "data", all_data);
    }

    send_json_response(out, response);
    json_decref(response);
    ;
}

// Function to handle POST request
void handle_post_request(FCGX_Request *request, const char *content_type, json_t *query_params)
{
    json_t *response = json_object();
    json_object_set_new(response, "method", json_string("POST"));

    const char *name_param = get_name_param(query_params);
    if (!name_param)
    {
        json_object_set_new(response, "error", json_string("Missing name parameter"));
        send_json_response(request->out, response);
        json_decref(response);
        return;
    }

    if (content_type && strstr(content_type, "application/json"))
    {
        const char *content_length_str = FCGX_GetParam("CONTENT_LENGTH", request->envp);
        if (content_length_str)
        {
            int content_length = atoi(content_length_str);
            if (content_length > 0 && content_length <= MAX_CONTENT_LENGTH)
            {
                char *post_data = malloc(content_length + 1);
                if (!post_data)
                {
                    json_object_set_new(response, "error", json_string("Memory allocation failed"));
                    send_json_response(request->out, response);
                    json_decref(response);
                    return;
                }

                if (post_data)
                {
                    int read_length = FCGX_GetStr(post_data, content_length, request->in);
                    if (read_length == content_length)
                    {
                        post_data[content_length] = '\0';
                        json_error_t error;
                        json_t *json_data = json_loads(post_data, 0, &error);
                        if (json_data)
                        {
                            // Set values based on name type
                            set_name_values(name_param, json_data);

                            // Save data to file
                            if (save_to_file(name_param, json_data) == 0)
                            {
                                json_object_set_new(response, "status", json_string("success"));
                                json_object_set_new(response, "name", json_string(name_param));
                                json_object_set_new(response, "data", json_deep_copy(json_data));
                            }
                            else
                            {
                                json_object_set_new(response, "error",
                                                    json_string("Failed to save data to file"));
                            }
                            json_decref(json_data);
                        }
                        else
                        {
                            json_object_set_new(response, "error", json_string(error.text));
                        }
                    }
                    free(post_data);
                }
            }
        }
    }
    else
    {
        json_object_set_new(response, "error",
                            json_string("Unsupported content type. Expected application/json"));
    }

    send_json_response(request->out, response);
    json_decref(response);
}

// Function to handle PUT request
void handle_put_request(FCGX_Request *request, const char *content_type, json_t *query_params)
{
    json_t *response = json_object();
    json_object_set_new(response, "method", json_string("PUT"));

    const char *name_param = get_name_param(query_params);
    if (!name_param)
    {
        json_object_set_new(response, "error", json_string("Missing name parameter"));
        send_json_response(request->out, response);
        json_decref(response);
        return;
    }

    // Check if file exists first
    json_t *existing_data = load_from_file(name_param);
    if (!existing_data)
    {
        json_object_set_new(response, "error", json_string("Resource not found"));
        send_json_response(request->out, response);
        json_decref(response);
        return;
    }
    json_decref(existing_data);

    if (content_type && strstr(content_type, "application/json"))
    {
        const char *content_length_str = FCGX_GetParam("CONTENT_LENGTH", request->envp);
        if (content_length_str)
        {
            int content_length = atoi(content_length_str);
            if (content_length > 0 && content_length <= MAX_CONTENT_LENGTH)
            {
                char *put_data = malloc(content_length + 1);
                if (put_data)
                {
                    int read_length = FCGX_GetStr(put_data, content_length, request->in);
                    if (read_length == content_length)
                    {
                        put_data[content_length] = '\0';
                        json_error_t error;
                        json_t *json_data = json_loads(put_data, 0, &error);
                        if (json_data)
                        {
                            // Set values based on name type (added)
                            set_name_values(name_param, json_data);

                            // Update file with new data
                            if (save_to_file(name_param, json_data) == 0)
                            {
                                json_object_set_new(response, "status", json_string("success"));
                                json_object_set_new(response, "name", json_string(name_param));
                                json_object_set_new(response, "data", json_deep_copy(json_data));
                            }
                            else
                            {
                                json_object_set_new(response, "error",
                                                    json_string("Failed to update data file"));
                            }
                            json_decref(json_data);
                        }
                        else
                        {
                            json_object_set_new(response, "error", json_string(error.text));
                        }
                    }
                    free(put_data);
                }
            }
        }
    }
    else
    {
        json_object_set_new(response, "error",
                            json_string("Unsupported content type. Expected application/json"));
    }

    send_json_response(request->out, response);
    json_decref(response);
}

// Function to handle DELETE request
void handle_delete_request(FCGX_Stream *out, json_t *query_params)
{
    json_t *response = json_object();
    json_object_set_new(response, "method", json_string("DELETE"));

    const char *name_param = get_name_param(query_params);
    if (!name_param)
    {
        json_object_set_new(response, "error", json_string("Missing name parameter"));
        send_json_response(out, response);
        json_decref(response);
        return;
    }

    // Create empty array for set_name_values
    json_t *empty_array = json_array();
    set_name_values(name_param, empty_array);
    json_decref(empty_array);

    // Create filename and attempt to delete
    char *filename = create_filename(name_param);
    if (!filename)
    {
        json_object_set_new(response, "error", json_string("Failed to create filename"));
        send_json_response(out, response);
        json_decref(response);
        return;
    }

    // Check if file exists before attempting deletion
    if (access(filename, F_OK) != 0)
    {
        json_object_set_new(response, "error", json_string("Resource not found"));
        free(filename);
        send_json_response(out, response);
        json_decref(response);
        return;
    }

    if (unlink(filename) == 0)
    {
        json_object_set_new(response, "status", json_string("success"));
        json_object_set_new(response, "name", json_string(name_param));
    }
    else
    {
        json_object_set_new(response, "error",
                            json_string("Failed to delete file"));
    }

    free(filename);
    send_json_response(out, response);
    json_decref(response);
}

void cleanup_fcgi_resources(FCGX_Request *request, int sock)
{
    if (request)
    {
        FCGX_Finish_r(request);
        FCGX_Free(request, 1);
    }
    if (sock >= 0)
    {
        close(sock);
    }
}

void *fcgi_thread_func(void *arg)
{
    (void)arg;
    int sock = -1;
    FCGX_Request request;
    json_t *query_params = NULL;
    char *socket_path = getenv(FCGI_SOCKET_NAME);
    int initialized = 0;

    if (!socket_path)
    {
        syslog(LOG_ERR, "Failed to get environment variable FCGI_SOCKET_NAME");
        goto cleanup;
    }

    // Initialize FastCGI library
    if (FCGX_Init() != 0)
    {
        syslog(LOG_ERR, "FCGX_Init failed");
        goto cleanup;
    }
    initialized = 1;

    // Open FastCGI socket
    sock = FCGX_OpenSocket(socket_path, 5);
    if (sock < 0)
    {
        syslog(LOG_ERR, "Failed to open FastCGI socket");
        goto cleanup;
    }

    // Set socket permissions
    if (chmod(socket_path, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
    {
        syslog(LOG_ERR, "Failed to set socket permissions: %s", strerror(errno));
        goto cleanup;
    }

    // Initialize request structure
    if (FCGX_InitRequest(&request, sock, 0) != 0)
    {
        syslog(LOG_ERR, "FCGX_InitRequest failed");
        goto cleanup;
    }

    // Ensure storage directory exists
    if (ensure_storage_directory() != 0)
    {
        syslog(LOG_ERR, "Failed to initialize storage directory");
        goto cleanup;
    }

    syslog(LOG_INFO, "FastCGI thread starting loop");

    // Main request handling loop with timeout
    while (fcgi_running)
    {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        // Set timeout to 1 second
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            if (errno != EINTR)
            {
                syslog(LOG_ERR, "select() failed: %s", strerror(errno));
                break;
            }
            continue;
        }

        // Check if we should exit
        if (!fcgi_running)
        {
            break;
        }

        // If no data available, continue loop
        if (ready == 0)
        {
            continue;
        }

        // Accept new request with timeout protection
        alarm(30); // Set 30-second timeout for accept
        int accept_result = FCGX_Accept_r(&request);
        alarm(0); // Clear timeout

        if (accept_result < 0)
        {
            if (errno != EINTR && errno != EAGAIN)
            {
                syslog(LOG_ERR, "FCGX_Accept_r failed: %d", accept_result);
                break;
            }
            continue;
        }

        const char *request_method = FCGX_GetParam("REQUEST_METHOD", request.envp);
        const char *content_type = FCGX_GetParam("CONTENT_TYPE", request.envp);
        const char *uri_string = FCGX_GetParam("REQUEST_URI", request.envp);

        if (!request_method || !uri_string)
        {
            json_t *error = json_object();
            json_object_set_new(error, "error", json_string("Invalid request parameters"));
            send_json_response(request.out, error);
            json_decref(error);
            FCGX_Finish_r(&request);
            continue;
        }

        // Parse URI and query parameters
        query_params = NULL;
        UriUriA uri;
        const char *errorPos;

        if (uriParseSingleUriA(&uri, uri_string, &errorPos) == URI_SUCCESS)
        {
            if (uri.query.first)
            {
                size_t query_length = uri.query.afterLast - uri.query.first;
                char *query = malloc(query_length + 1);

                if (query)
                {
                    strncpy(query, uri.query.first, query_length);
                    query[query_length] = '\0';
                    query_params = parse_query_params(query);
                    free(query);
                }
                else
                {
                    syslog(LOG_ERR, "Failed to allocate memory for query string");
                }
            }
            uriFreeUriMembersA(&uri);
        }
        else
        {
            syslog(LOG_ERR, "Failed to parse URI: %s", uri_string);
        }

        // Handle request based on method with timeout protection
        alarm(30); // Set 30-second timeout for request handling

        if (strcmp(request_method, "GET") == 0)
        {
            handle_get_request(request.out, query_params);
        }
        else if (strcmp(request_method, "POST") == 0)
        {
            handle_post_request(&request, content_type, query_params);
        }
        else if (strcmp(request_method, "PUT") == 0)
        {
            handle_put_request(&request, content_type, query_params);
        }
        else if (strcmp(request_method, "DELETE") == 0)
        {
            handle_delete_request(request.out, query_params);
        }
        else
        {
            json_t *error = json_object();
            json_object_set_new(error, "error", json_string("Unsupported method"));
            send_json_response(request.out, error);
            json_decref(error);
        }

        alarm(0); // Clear the timeout

        // Cleanup query parameters
        if (query_params)
        {
            json_decref(query_params);
            query_params = NULL;
        }

        FCGX_Finish_r(&request);
    }

cleanup:
    syslog(LOG_INFO, "FastCGI thread stopping");

    if (query_params)
    {
        json_decref(query_params);
    }

    if (sock >= 0)
    {
        close(sock);
    }

    if (initialized)
    {
        FCGX_Finish_r(&request);
    }

    return NULL;
}