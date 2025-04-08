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

#pragma once

#include <axoverlay.h>
#include <cairo/cairo.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>

#include "detection.h"
#include "counting.h"
#include "incident.h"

/***** Drawing functions *****************************************************/

/**
 * brief Converts palette color index to cairo color value.
 *
 * This function converts the palette index, which has been initialized by
 * function axoverlay_set_palette_color to a value that can be used by
 * function cairo_set_source_rgba.
 *
 * param color_index Index in the palette setup.
 *
 * return color value.
 */
gdouble index2cairo(const gint color_index);

/**
 * brief Setup an overlay_data struct.
 *
 * This function initialize and setup an overlay_data
 * struct with default values.
 *
 * param data The overlay data struct to initialize.
 */
void setup_axoverlay_data(struct axoverlay_overlay_data *data);

/**
 * brief Setup palette color table.
 *
 * This function initialize and setup an palette index
 * representing ARGB values.
 *
 * param color_index Palette color index.
 * param r R (red) value.
 * param g G (green) value.
 * param b B (blue) value.
 * param a A (alpha) value.
 *
 * return result as boolean
 */
gboolean
setup_palette_color(const int index, const gint r, const gint g, const gint b, const gint a);

/***** Callback functions ****************************************************/

/**
 * brief A callback function called when an overlay needs adjustments.
 *
 * This function is called to let developers make adjustments to
 * the size and position of their overlays for each stream. This callback
 * function is called prior to rendering every time when an overlay
 * is rendered on a stream, which is useful if the resolution has been
 * updated or rotation has changed.
 *
 * param id Overlay id.
 * param stream Information about the rendered stream.
 * param postype The position type.
 * param overlay_x The x coordinate of the overlay.
 * param overlay_y The y coordinate of the overlay.
 * param overlay_width Overlay width.
 * param overlay_height Overlay height.
 * param user_data Optional user data associated with this overlay.
 */
void adjustment_cb(gint id,
                   struct axoverlay_stream_data *stream,
                   enum axoverlay_position_type *postype,
                   gfloat *overlay_x,
                   gfloat *overlay_y,
                   gint *overlay_width,
                   gint *overlay_height,
                   gpointer user_data);

bool load_vehicle_icons(void);
void cleanup_vehicle_icons(void);
void draw_vehicle_icon(cairo_t *rendering_context, double x, double y, int index);
void draw_count(cairo_t *rendering_context, gint width, gint height, gint line_width);

// Event-related functions
const char *get_event_type_name(EventType type);
Event *find_object_event(int object_id);

// Updated draw_label_overlay with object_id parameter
void draw_label_overlay(cairo_t *rendering_context, gint width, gint height,
                        double top, double left, double bottom, double right,
                        const gchar *text, const gchar *id,
                        double r, double g, double b, int object_id);

void draw_label(cairo_t *rendering_context, gint width, gint height);
void draw_roi_polygon(cairo_t *rendering_context, gint width, gint height, gint line_width);
void draw_counting_line(cairo_t *rendering_context,
                        gint width, gint height,
                        gint line_width,
                        CountingSystem *system);
void draw_transparent(cairo_t *rendering_context, gint x, gint y, gint width, gint height);