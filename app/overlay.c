/**
 * Copyright (C) 2021, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * - axoverlay -
 *
 * This application demonstrates how the use the API axoverlay, by drawing
 * plain boxes using 4-bit palette color format and text overlay using
 * ARGB32 color format.
 *
 * Colorspace and alignment:
 * 1-bit palette (AXOVERLAY_COLORSPACE_1BIT_PALETTE): 32-byte alignment
 * 4-bit palette (AXOVERLAY_COLORSPACE_4BIT_PALETTE): 16-byte alignment
 * ARGB32 (AXOVERLAY_COLORSPACE_ARGB32): 16-byte alignment
 *
 */

#include "overlay.h"
#include "incident.h" // Added include for event functionality
#include <syslog.h>

#define PALETTE_VALUE_RANGE 255.0

// Global variables
static cairo_surface_t *vehicle_icons[7] = {NULL};

// Define colors at the top with other globals
static const gdouble vehicle_colors[9][3] = {
    {0.00, 0.70, 0.40}, // Bright green for CAR
    {0.00, 0.55, 0.90}, // Strong blue for BIKE
    {0.20, 0.80, 0.70}, // Sea foam for TRUCK
    {0.90, 0.60, 0.10}, // Golden orange for BUS
    {0.75, 0.75, 0.00}, // Rich yellow for TAXI
    {0.60, 0.30, 0.80}, // Soft purple for PICKUP
    {0.85, 0.50, 0.00}, // Deep brown for TRAILER
    {0.40, 0.40, 0.40}, // Medium gray for PERSON
    {0.00, 0.60, 0.50}  // Teal for CONE
};

/***** Drawing functions *****************************************************/

/**
 * @brief Converts palette color index to cairo color value.
 *
 * This function converts the palette index, which has been initialized by
 * function axoverlay_set_palette_color to a value that can be used by
 * function cairo_set_source_rgba.
 *
 * @param color_index Index in the palette setup.
 *
 * return color value.
 */
gdouble index2cairo(const gint color_index)
{
    return ((color_index << 4) + color_index) / PALETTE_VALUE_RANGE;
}

/**
 * @brief Setup an overlay_data struct.
 *
 * This function initialize and setup an overlay_data
 * struct with default values.
 *
 * @param data The overlay data struct to initialize.
 */
void setup_axoverlay_data(struct axoverlay_overlay_data *data)
{
    axoverlay_init_overlay_data(data);
    data->postype = AXOVERLAY_CUSTOM_NORMALIZED;
    data->anchor_point = AXOVERLAY_ANCHOR_CENTER;
    data->x = 0.0;
    data->y = 0.0;
    data->scale_to_stream = FALSE;
}

/**
 * @brief Setup palette color table.
 *
 * This function initialize and setup an palette index
 * representing ARGB values.
 *
 * @param color_index Palette color index.
 * @param r R (red) value.
 * @param g G (green) value.
 * @param b B (blue) value.
 * @param a A (alpha) value.
 *
 * return result as boolean
 */
gboolean
setup_palette_color(const int index, const gint r, const gint g, const gint b, const gint a)
{
    GError *overlayError = NULL;
    struct axoverlay_palette_color color;

    color.red = r;
    color.green = g;
    color.blue = b;
    color.alpha = a;
    color.pixelate = FALSE;
    axoverlay_set_palette_color(index, &color, &overlayError);
    if (overlayError != NULL)
    {
        g_error_free(overlayError);
        return FALSE;
    }

    return TRUE;
}

/***** Callback functions ****************************************************/

/**
 * @brief A callback function called when an overlay needs adjustments.
 *
 * This function is called to let developers make adjustments to
 * the size and position of their overlays for each stream. This callback
 * function is called prior to rendering every time when an overlay
 * is rendered on a stream, which is useful if the resolution has been
 * updated or rotation has changed.
 *
 * @param id Overlay id.
 * @param stream Information about the rendered stream.
 * @param postype The position type.
 * @param overlay_x The x coordinate of the overlay.
 * @param overlay_y The y coordinate of the overlay.
 * @param overlay_width Overlay width.
 * @param overlay_height Overlay height.
 * @param user_data Optional user data associated with this overlay.
 */
void adjustment_cb(gint id,
                   struct axoverlay_stream_data *stream,
                   enum axoverlay_position_type *postype,
                   gfloat *overlay_x,
                   gfloat *overlay_y,
                   gint *overlay_width,
                   gint *overlay_height,
                   gpointer user_data)
{
    /* Silence compiler warnings for unused @parameters/arguments */
    (void)id;
    (void)postype;
    (void)overlay_x;
    (void)overlay_y;
    (void)user_data;

    // syslog(LOG_INFO, "Adjust callback for overlay: %i x %i", *overlay_width, *overlay_height);
    // syslog(LOG_INFO, "Adjust callback for stream: %i x %i", stream->width, stream->height);

    *overlay_width = stream->width;
    *overlay_height = stream->height;
}

// Function to load vehicle icons
bool load_vehicle_icons(void)
{
    const char *icon_paths[] = {
        "/usr/local/packages/enixma_analytic/html/icons/car.png",
        "/usr/local/packages/enixma_analytic/html/icons/motorbike.png",
        "/usr/local/packages/enixma_analytic/html/icons/truck.png",
        "/usr/local/packages/enixma_analytic/html/icons/bus.png",
        "/usr/local/packages/enixma_analytic/html/icons/taxi.png",
        "/usr/local/packages/enixma_analytic/html/icons/pickup.png",
        "/usr/local/packages/enixma_analytic/html/icons/trailer.png"};

    for (int i = 0; i < 7; i++)
    {
        vehicle_icons[i] = cairo_image_surface_create_from_png(icon_paths[i]);
        cairo_status_t status = cairo_surface_status(vehicle_icons[i]);
        // int width = cairo_image_surface_get_width(vehicle_icons[i]);
        // int height = cairo_image_surface_get_height(vehicle_icons[i]);
        // syslog(LOG_INFO, "Icon %d: Status=%d, Size=%dx%d, Path=%s",
        //        i, status, width, height, icon_paths[i]);

        if (status != CAIRO_STATUS_SUCCESS)
        {
            syslog(LOG_ERR, "Failed to load icon: %s (Status: %s)",
                   icon_paths[i], cairo_status_to_string(status));
            for (int j = 0; j < i; j++)
            {
                if (vehicle_icons[j])
                {
                    cairo_surface_destroy(vehicle_icons[j]);
                    vehicle_icons[j] = NULL;
                }
            }
            return false;
        }
    }
    return true;
}

// Function to cleanup icons
void cleanup_vehicle_icons(void)
{
    for (int i = 0; i < 7; i++)
    {
        if (vehicle_icons[i])
        {
            cairo_surface_destroy(vehicle_icons[i]);
            vehicle_icons[i] = NULL;
        }
    }
}

// Function to draw a vehicle icon
void draw_vehicle_icon(cairo_t *rendering_context, double x, double y, int index)
{
    if (!vehicle_icons[index])
    {
        syslog(LOG_ERR, "Attempted to draw NULL icon at index %d", index);
        return;
    }

    cairo_save(rendering_context);

    // Change operator
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_OVER);

    int width = cairo_image_surface_get_width(vehicle_icons[index]);
    int height = cairo_image_surface_get_height(vehicle_icons[index]);

    // syslog(LOG_INFO, "Drawing icon %d: pos=(%f,%f), size=%dx%d",
    //        index, x, y, width, height);

    // Set source surface
    cairo_set_source_surface(rendering_context, vehicle_icons[index],
                             x - (width / 2),
                             y - (height / 2));

    // Ensure pattern is set properly
    cairo_pattern_set_extend(cairo_get_source(rendering_context), CAIRO_EXTEND_NONE);

    // Paint with masking
    cairo_mask_surface(rendering_context, vehicle_icons[index],
                       x - (width / 2),
                       y - (height / 2));

    cairo_restore(rendering_context);
}

// Draw vehicle count display
void draw_count(cairo_t *rendering_context, gint width, gint height, gint line_width)
{
    if (!counting_system)
        return;

    int num_vehicles = 7;
    double divide = width / num_vehicles;
    double pre_divide = 0;

    // Get counts from both lines for all classes
    double counts[num_vehicles];
    for (int i = 0; i < num_vehicles; i++)
    {
        counts[i] = 0;

        // Sum up counts from all lanes in LINE_1
        for (int lane_id = 0; lane_id < counting_system->line1.num_lanes; lane_id++)
        {
            int up_count = 0, down_count = 0;

            // Only get counts if class_id is valid (less than num_classes)
            if (i < counting_system->num_classes)
            {
                get_lane_counts(counting_system, LINE_1, i, lane_id, &up_count, &down_count);
            }

            counts[i] += up_count + down_count;
        }

        // Sum up counts from all lanes in LINE_2 (if active)
        if (counting_system->use_second_line)
        {
            for (int lane_id = 0; lane_id < counting_system->line2.num_lanes; lane_id++)
            {
                int up_count = 0, down_count = 0;

                // Only get counts if class_id is valid (less than num_classes)
                if (i < counting_system->num_classes)
                {
                    get_lane_counts(counting_system, LINE_2, i, lane_id, &up_count, &down_count);
                }

                counts[i] += up_count + down_count;
            }
        }
    }

    for (int i = 0; i < num_vehicles; i++)
    {
        double divideWidth = (i < num_vehicles - 1) ? divide : width - pre_divide;

        // Draw colored rectangle
        cairo_set_source_rgba(rendering_context,
                              vehicle_colors[i][0],
                              vehicle_colors[i][1],
                              vehicle_colors[i][2],
                              0.7);
        cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
        cairo_set_line_width(rendering_context, line_width);
        cairo_rectangle(rendering_context, pre_divide, 0, divideWidth, height * 3 / 80);
        cairo_fill(rendering_context);

        // Draw dark gray rectangle
        cairo_set_source_rgba(rendering_context, 0.1, 0.1, 0.1, 0.7);
        cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
        cairo_set_line_width(rendering_context, line_width);
        cairo_rectangle(rendering_context, pre_divide + (width / 40), height * 3 / 80, divide - (width / 20), height * 3 / 80);
        cairo_fill(rendering_context);

        // Calculate center position
        double x = divide * (2 * i + 1) / 2;

        // Draw vehicle icon
        draw_vehicle_icon(rendering_context,
                          x,                 // center x
                          height * 23 / 400, // center y
                          i);

        // Draw count
        cairo_set_source_rgb(rendering_context, 1, 1, 1);
        cairo_select_font_face(rendering_context, "serif", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(rendering_context, height * 7 / 200);

        char *count_str = NULL;
        int result = asprintf(&count_str, "%g", counts[i]);
        if (result != -1)
        {
            cairo_text_extents_t count_te;
            cairo_text_extents(rendering_context, count_str, &count_te);
            cairo_move_to(rendering_context, x - count_te.width / 2, height / 32);
            cairo_show_text(rendering_context, count_str);
            free(count_str);
        }

        pre_divide = divide * (i + 1);
    }
}

// Convert event type to string
const char *get_event_type_name(EventType type)
{
    switch (type)
    {
    case EVENT_CAR_STOPPED:
        return "CAR STOPPED";
    case EVENT_CAR_BROKEN:
        return "CAR BROKEN";
    case EVENT_CAR_ACCIDENT:
        return "CAR ACCIDENT";
    case EVENT_ROAD_BLOCKED:
        return "ROAD BLOCKED";
    case EVENT_ROAD_CONSTRUCTION:
        return "ROAD CONSTRUCTION";
    default:
        return "UNKNOWN EVENT";
    }
}

// Find if an object has an active event
Event *find_object_event(int object_id)
{
    for (int i = 0; i < event_count; i++)
    {
        if (event_list[i].object_id == object_id)
        {
            return &event_list[i];
        }
    }
    return NULL;
}

// Draw label with event detection
void draw_label_overlay(cairo_t *rendering_context, gint width, gint height,
                        double top, double left, double bottom, double right,
                        const gchar *text, const gchar *id,
                        double r, double g, double b, int object_id)
{
    gint newHeight = width * (10.0 / 16.0);

    cairo_text_extents_t te;
    cairo_text_extents(rendering_context, text, &te);

    // Draw the label background
    cairo_set_source_rgb(rendering_context, r, g, b);
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width(rendering_context, 3.0);
    cairo_rectangle(rendering_context, left * width, top * newHeight + (height - newHeight) / 2 - te.height - (newHeight / 100), te.width, te.height + (newHeight / 100));
    cairo_fill(rendering_context);

    // Draw the bounding box
    cairo_rectangle(rendering_context, left * width, top * newHeight + (height - newHeight) / 2, (right - left) * width, (bottom - top) * newHeight);
    cairo_stroke(rendering_context);

    // Draw the object label (speed)
    cairo_set_source_rgb(rendering_context, 1, 1, 1);
    cairo_move_to(rendering_context, left * width, top * newHeight + (height - newHeight) / 2 - (newHeight / 160));
    cairo_show_text(rendering_context, text);

    // Set font size for ID
    cairo_set_font_size(rendering_context, height * 3 / 200);

    // Draw the ID
    cairo_text_extents_t id_te;
    cairo_text_extents(rendering_context, id, &id_te);
    cairo_move_to(rendering_context, left * width, top * newHeight + (height - newHeight) / 2 + (newHeight * 3 / 160));
    cairo_show_text(rendering_context, id);

    // Check if this object has an active event
    Event *event = find_object_event(object_id);

    // Create event text if applicable
    gchar *event_text = NULL;
    if (event != NULL)
    {
        // Use red color for objects with events
        r = 1.0;
        g = 0.0;
        b = 0.0;

        // Create the event text
        event_text = g_strdup_printf(" %s  ", get_event_type_name(event->type));
    }

    // Draw event text if applicable
    if (event_text != NULL)
    {
        // Set font size for event text (slightly larger)
        cairo_set_font_size(rendering_context, height * 4 / 200);

        // Draw event text above bounding box
        cairo_text_extents_t event_te;
        cairo_text_extents(rendering_context, event_text, &event_te);

        // Draw background for event text
        cairo_set_source_rgb(rendering_context, r, g, b);
        cairo_rectangle(rendering_context, left * width,
                        top * newHeight + (height - newHeight) / 2 - te.height - (newHeight / 40) - event_te.height,
                        event_te.width, event_te.height + (newHeight / 100));
        cairo_fill(rendering_context);

        // Draw event text
        cairo_set_source_rgb(rendering_context, 1, 1, 1);
        cairo_move_to(rendering_context, left * width,
                      top * newHeight + (height - newHeight) / 2 - te.height - (newHeight / (160 / 3)));
        cairo_show_text(rendering_context, event_text);

        // Free the event text
        g_free(event_text);
    }
}

void draw_label(cairo_t *rendering_context, gint width, gint height)
{
    if (!tracker)
        return;

    gint newHeight = width * (10.0 / 16.0);

    for (int i = 0; i < tracker->count; i++)
    {
        float r = vehicle_colors[tracker->objects[i].class_id][0];
        float g = vehicle_colors[tracker->objects[i].class_id][1];
        float b = vehicle_colors[tracker->objects[i].class_id][2];

        if (tracker->objects[i].time_since_update > tracker->max_age)
            continue;
        if (tracker->objects[i].hits < tracker->min_hits)
            continue;

        for (int j = 0; j < tracker->objects[i].trajectory_count; j++)
        {
            double old_cx = tracker->objects[i].trajectory[j - 1].x * width;
            double old_cy = tracker->objects[i].trajectory[j - 1].y * newHeight + (height - newHeight) / 2;

            double new_cx = tracker->objects[i].trajectory[j].x * width;
            double new_cy = tracker->objects[i].trajectory[j].y * newHeight + (height - newHeight) / 2;

            // Check if the old center is effectively non-zero
            if (fabs(old_cx) > EPSILON && fabs(old_cy) > EPSILON)
            {
                cairo_set_source_rgba(rendering_context, r, g, b, 0.5); // Line color
                cairo_set_line_width(rendering_context, 2.0);
                cairo_move_to(rendering_context, old_cx, old_cy); // Start of the line (old center)
                cairo_line_to(rendering_context, new_cx, new_cy); // End of the line (new center)
                cairo_stroke(rendering_context);
            }
        }

        // float score  = tracker->objects[i].score;
        float top = tracker->objects[i].bbox[0];
        float left = tracker->objects[i].bbox[1];
        float bottom = tracker->objects[i].bbox[2];
        float right = tracker->objects[i].bbox[3];

        // gchar *label = g_strdup_printf(" %s: %.2f km/h  ", context.label.labels[tracker->objects[i].class_id], tracker->objects[i].speed_kmh);
        gchar *label;
        if (tracker->objects[i].speed_kmh < 0.01)
        {
            label = g_strdup_printf(" %s  ", context.label.labels[tracker->objects[i].class_id]);
        }
        else
        {
            label = g_strdup_printf(" %s: %.2f km/h  ", context.label.labels[tracker->objects[i].class_id], tracker->objects[i].speed_kmh);
        }
        gchar *id = g_strdup_printf(" id: %i", tracker->objects[i].track_id);

        //  Show text in black
        cairo_set_source_rgb(rendering_context, 0, 0, 0);
        cairo_select_font_face(rendering_context, "serif", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(rendering_context, height / 50);

        // Pass the track_id as the last parameter for event detection
        draw_label_overlay(rendering_context, width, height, top, left, bottom, right,
                           label, id, r, g, b, tracker->objects[i].track_id);

        // Free temporary strings
        g_free(label);
        g_free(id);
    }
}

void draw_roi_polygon(cairo_t *rendering_context, gint width, gint height, gint line_width)
{
    Polygon *rois[] = {roi1, roi2};

    // First pass: Fill both polygons
    cairo_set_source_rgba(rendering_context, 0.0, 0.0, 0.0, 0.0);
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width(rendering_context, line_width);

    for (int roi_idx = 0; roi_idx < 2; roi_idx++)
    {
        if (rois[roi_idx] != NULL && rois[roi_idx]->points != NULL)
        {
            // Move to first point
            int first_x = (rois[roi_idx]->points[0].x * width);
            int first_y = (rois[roi_idx]->points[0].y * height);
            cairo_move_to(rendering_context, first_x, first_y);

            for (int i = 0; i < rois[roi_idx]->num_points; i++)
            {
                int point_x = (rois[roi_idx]->points[i].x * width);
                int point_y = (rois[roi_idx]->points[i].y * height);
                cairo_line_to(rendering_context, point_x, point_y);
            }
            cairo_close_path(rendering_context);
            cairo_fill(rendering_context);
        }
    }

    // Second pass: Draw borders for both polygons
    cairo_set_source_rgb(rendering_context, 1.0, 0.0, 0.0);
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width(rendering_context, line_width);

    for (int roi_idx = 0; roi_idx < 2; roi_idx++)
    {
        if (rois[roi_idx] != NULL && rois[roi_idx]->points != NULL)
        {
            // Move to first point
            int first_x = (rois[roi_idx]->points[0].x * width);
            int first_y = (rois[roi_idx]->points[0].y * height);
            cairo_move_to(rendering_context, first_x, first_y);

            for (int i = 0; i < rois[roi_idx]->num_points; i++)
            {
                int point_x = (rois[roi_idx]->points[i].x * width);
                int point_y = (rois[roi_idx]->points[i].y * height);
                cairo_line_to(rendering_context, point_x, point_y);
            }
            cairo_close_path(rendering_context);
            cairo_stroke(rendering_context);
        }
    }
}

// Draw counting lines with dots and arrows
void draw_counting_line(cairo_t *rendering_context,
                        gint width, gint height, gint line_width,
                        CountingSystem *system)
{
    if (!system)
        return;

    // Set up colors for normal (reds) and counting (green) states
    gdouble normal_color[] = {1.0, 0.0, 0.0}; // Red for normal state
    gdouble active_color[] = {0.0, 1.0, 0.0}; // Green for active counting

    // Set line properties
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width(rendering_context, line_width); // Make line thicker

    // Draw first line with multiple segments
    for (int i = 0; i < system->line1.num_lanes; i++)
    {
        cairo_set_source_rgb(rendering_context, normal_color[0], normal_color[1], normal_color[2]);

        // Draw circles at each point
        for (int j = i; j <= i + 1; j++)
        {
            // Skip drawing if BOTH x AND y are close to 0
            if (fabs(system->line1.points[j].x) < EPSILON && fabs(system->line1.points[j].y) < EPSILON)
                continue;

            cairo_arc(rendering_context,
                      system->line1.points[j].x * width,
                      system->line1.points[j].y * height,
                      5.0, 0, 2 * G_PI);
            cairo_fill(rendering_context);
        }

        // Check if any counting occurred in this lane recently
        bool lane_counting = system->line1.timestamps[i] > 0 &&
                             (g_get_monotonic_time() - system->line1.timestamps[i]) < 250000; // 250ms blink

        // Set color based on counting state
        if (lane_counting)
        {
            cairo_set_source_rgb(rendering_context, active_color[0], active_color[1], active_color[2]);
        }

        // Skip drawing line segment if both end points have x and y close to 0
        if ((fabs(system->line1.points[i].x) < EPSILON && fabs(system->line1.points[i].y) < EPSILON) ||
            (fabs(system->line1.points[i + 1].x) < EPSILON && fabs(system->line1.points[i + 1].y) < EPSILON))
        {
            continue;
        }

        // Draw line segment
        cairo_move_to(rendering_context,
                      system->line1.points[i].x * width,
                      system->line1.points[i].y * height);
        cairo_line_to(rendering_context,
                      system->line1.points[i + 1].x * width,
                      system->line1.points[i + 1].y * height);
        cairo_stroke(rendering_context);

        // Draw arrow below the line
        float mid_x = (system->line1.points[i].x + system->line1.points[i + 1].x) * width / 2;
        float mid_y = (system->line1.points[i].y + system->line1.points[i + 1].y) * height / 2;
        float arrow_size = line_width * 5;

        // Calculate the angle of the line segment
        float dx = system->line1.points[i + 1].x - system->line1.points[i].x;
        float dy = system->line1.points[i + 1].y - system->line1.points[i].y;

        // Reverse the direction if needed
        if (!system->line1_direction)
        {
            dx = -dx;
            dy = -dy;
        }

        float angle = atan2(dy, dx);

        // Rotate the original V-shaped arrow to match line direction
        // Original V points were at (-arrow_size, +arrow_size), (0, +arrow_size*2), and (+arrow_size, +arrow_size)
        // relative to mid_x, mid_y

        // Calculate rotated points
        float left_x = mid_x + (-arrow_size * cos(angle) - arrow_size * sin(angle));
        float left_y = mid_y + (-arrow_size * sin(angle) + arrow_size * cos(angle));

        float center_x = mid_x + (0 * cos(angle) - arrow_size * 2 * sin(angle));
        float center_y = mid_y + (0 * sin(angle) + arrow_size * 2 * cos(angle));

        float right_x = mid_x + (arrow_size * cos(angle) - arrow_size * sin(angle));
        float right_y = mid_y + (arrow_size * sin(angle) + arrow_size * cos(angle));

        // Draw V-shaped arrow with same shape but rotated to match line
        cairo_move_to(rendering_context, left_x, left_y);
        cairo_line_to(rendering_context, center_x, center_y);
        cairo_line_to(rendering_context, right_x, right_y);
        cairo_stroke(rendering_context);

        // Get total count for this lane (sum all classes)
        int lane_total = 0;
        for (int class_id = 0; class_id < system->num_classes; class_id++)
        {
            int up_count = 0, down_count = 0;
            get_lane_counts(system, LINE_1, class_id, i, &up_count, &down_count);
            lane_total += up_count + down_count;
        }

        // Display count based on line2_direction
        cairo_set_source_rgb(rendering_context, 1, 1, 1); // White text
        cairo_select_font_face(rendering_context, "serif", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(rendering_context, height * 7 / 200); // Slightly larger text

        char *count_lane = NULL;
        int lane_result = asprintf(&count_lane, "%d", lane_total);
        if (lane_result != -1)
        {
            cairo_text_extents_t count_lane_te;
            cairo_text_extents(rendering_context, count_lane, &count_lane_te);

            float text_y_pos;
            float rect_y_pos;

            if (!system->line1_direction)
            {
                // Below the lane
                rect_y_pos = mid_y + arrow_size;
                text_y_pos = mid_y + arrow_size + count_lane_te.height + 5;
            }
            else
            {
                // Above the lane
                rect_y_pos = mid_y - arrow_size - count_lane_te.height - 10;
                text_y_pos = mid_y - arrow_size - 5;
            }

            // Draw text with semi-transparent background
            cairo_set_source_rgba(rendering_context, 0.0, 0.0, 0.0, 0.5); // Semi-transparent black background
            cairo_rectangle(rendering_context,
                            mid_x - count_lane_te.width / 2 - 5,
                            rect_y_pos,
                            count_lane_te.width + 10,
                            count_lane_te.height + 10);
            cairo_fill(rendering_context);

            cairo_set_source_rgb(rendering_context, 1, 1, 1); // White text
            cairo_move_to(rendering_context,
                          mid_x - count_lane_te.width / 2,
                          text_y_pos);
            cairo_show_text(rendering_context, count_lane);
            free(count_lane);
        }
    }

    // Draw second line if enabled - same approach as above
    if (system->use_second_line)
    {
        for (int i = 0; i < system->line2.num_lanes; i++)
        {
            cairo_set_source_rgb(rendering_context, normal_color[0], normal_color[1], normal_color[2]);

            // Draw circles at each point
            for (int j = i; j <= i + 1; j++)
            {
                // Skip drawing if BOTH x AND y are close to 0
                if (fabs(system->line2.points[j].x) < EPSILON && fabs(system->line2.points[j].y) < EPSILON)
                    continue;

                cairo_arc(rendering_context,
                          system->line2.points[j].x * width,
                          system->line2.points[j].y * height,
                          5.0, 0, 2 * G_PI);
                cairo_fill(rendering_context);
            }

            // Check if any counting occurred in this lane recently
            bool lane_counting = system->line2.timestamps[i] > 0 &&
                                 (g_get_monotonic_time() - system->line2.timestamps[i]) < 250000;

            if (lane_counting)
            {
                cairo_set_source_rgb(rendering_context, active_color[0], active_color[1], active_color[2]);
            }

            // Skip drawing line segment if both end points have x and y close to 0
            if ((fabs(system->line2.points[i].x) < EPSILON && fabs(system->line2.points[i].y) < EPSILON) ||
                (fabs(system->line2.points[i + 1].x) < EPSILON && fabs(system->line2.points[i + 1].y) < EPSILON))
            {
                continue;
            }

            // Draw line segment
            cairo_move_to(rendering_context,
                          system->line2.points[i].x * width,
                          system->line2.points[i].y * height);
            cairo_line_to(rendering_context,
                          system->line2.points[i + 1].x * width,
                          system->line2.points[i + 1].y * height);
            cairo_stroke(rendering_context);

            // Draw arrow below the line
            float mid_x = (system->line2.points[i].x + system->line2.points[i + 1].x) * width / 2;
            float mid_y = (system->line2.points[i].y + system->line2.points[i + 1].y) * height / 2;
            float arrow_size = line_width * 5;

            // Calculate the angle of the line segment
            float dx = system->line2.points[i + 1].x - system->line2.points[i].x;
            float dy = system->line2.points[i + 1].y - system->line2.points[i].y;

            // Reverse the direction if needed
            if (!system->line2_direction)
            {
                dx = -dx;
                dy = -dy;
            }

            float angle = atan2(dy, dx);

            // Rotate the original V-shaped arrow to match line direction
            // Original V points were at (-arrow_size, +arrow_size), (0, +arrow_size*2), and (+arrow_size, +arrow_size)
            // relative to mid_x, mid_y

            // Calculate rotated points
            float left_x = mid_x + (-arrow_size * cos(angle) - arrow_size * sin(angle));
            float left_y = mid_y + (-arrow_size * sin(angle) + arrow_size * cos(angle));

            float center_x = mid_x + (0 * cos(angle) - arrow_size * 2 * sin(angle));
            float center_y = mid_y + (0 * sin(angle) + arrow_size * 2 * cos(angle));

            float right_x = mid_x + (arrow_size * cos(angle) - arrow_size * sin(angle));
            float right_y = mid_y + (arrow_size * sin(angle) + arrow_size * cos(angle));

            // Draw V-shaped arrow with same shape but rotated to match line
            cairo_move_to(rendering_context, left_x, left_y);
            cairo_line_to(rendering_context, center_x, center_y);
            cairo_line_to(rendering_context, right_x, right_y);
            cairo_stroke(rendering_context);

            // Get total count for this lane
            int lane_total = 0;
            for (int class_id = 0; class_id < system->num_classes; class_id++)
            {
                int up_count = 0, down_count = 0;
                get_lane_counts(system, LINE_2, class_id, i, &up_count, &down_count);
                lane_total += up_count + down_count;
            }

            // Display count based on line2_direction
            cairo_set_source_rgb(rendering_context, 1, 1, 1);
            cairo_select_font_face(rendering_context, "serif", CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(rendering_context, height * 7 / 200);

            char *count_lane = NULL;
            int lane_result = asprintf(&count_lane, "%d", lane_total);
            if (lane_result != -1)
            {
                cairo_text_extents_t count_lane_te;
                cairo_text_extents(rendering_context, count_lane, &count_lane_te);

                // Position based on line2_direction
                float text_y_pos;
                float rect_y_pos;

                if (!system->line2_direction)
                {
                    // Below the lane
                    rect_y_pos = mid_y + arrow_size;
                    text_y_pos = mid_y + arrow_size + count_lane_te.height + 5;
                }
                else
                {
                    // Above the lane
                    rect_y_pos = mid_y - arrow_size - count_lane_te.height - 10;
                    text_y_pos = mid_y - arrow_size - 5;
                }

                // Draw text with semi-transparent background
                cairo_set_source_rgba(rendering_context, 0.0, 0.0, 0.0, 0.5);
                cairo_rectangle(rendering_context,
                                mid_x - count_lane_te.width / 2 - 5,
                                rect_y_pos,
                                count_lane_te.width + 10,
                                count_lane_te.height + 10);
                cairo_fill(rendering_context);

                cairo_set_source_rgb(rendering_context, 1, 1, 1);
                cairo_move_to(rendering_context,
                              mid_x - count_lane_te.width / 2,
                              text_y_pos);
                cairo_show_text(rendering_context, count_lane);
                free(count_lane);
            }
        }
    }
}

void draw_transparent(cairo_t *rendering_context, gint x, gint y, gint width, gint height)
{
    cairo_set_source_rgba(rendering_context, 0.0, 0.0, 0.0, 0.2);
    cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
    cairo_rectangle(rendering_context, x, y, width, height);
    cairo_fill(rendering_context);
}