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
 * - send_event.c -
 *
 * This example illustrates how to send a stateful ONVIF event, which is
 * changing the value every 10th second.
 *
 * Error handling has been omitted for the sake of brevity.
 */
#include "event.h"
#include "deepsort.h"
#include "detection.h"
#include <string.h>
#include <syslog.h>

AppData_StopLine *app_data_stopline = NULL;
AppData_Counting *app_data_counting = NULL;
AppData_Incidents *app_data_incidents = NULL;

/**
 * Send stop line event with current vehicle and incident data
 * Includes class data as comma-separated strings
 */
gboolean send_event_stopline(AppData_StopLine *app_data)
{
    AXEventKeyValueSet *key_value_set = NULL;
    AXEvent *event = NULL;

    // Update current data
    app_data->total_vehicles = tracker->count;

    // Create strings to hold arrays as comma-separated values
    GString *class_labels_str = g_string_new("[");
    GString *class_ids_str = g_string_new("[");

    // Counter for valid objects
    int valid_count = 0;

    // Iterate through all tracked objects and collect their data
    for (int i = 0; i < tracker->count; i++)
    {
        // Only include objects that meet minimum hits criteria
        if (tracker->objects[i].hits >= tracker->min_hits)
        {
            // Add separators after the first element
            if (valid_count > 0)
            {
                g_string_append(class_labels_str, ",");
                g_string_append(class_ids_str, ",");
            }

            // Get the class label for this object
            const char *label = context.label.labels[tracker->objects[i].class_id];

            // Append class label
            g_string_append(class_labels_str, label);

            // Append class ID
            g_string_append_printf(class_ids_str, "%d", tracker->objects[i].class_id);

            valid_count++;
        }
    }

    g_string_append(class_labels_str, "]");
    g_string_append(class_ids_str, "]");

    key_value_set = ax_event_key_value_set_new();

    // Add the vehicle count to the event
    ax_event_key_value_set_add_key_value(
        key_value_set, "total_vehicles", NULL,
        &app_data->total_vehicles, AX_VALUE_TYPE_INT, NULL);

    // Add the string representations to the event
    ax_event_key_value_set_add_key_value(
        key_value_set, "class_labels", NULL,
        class_labels_str->str, AX_VALUE_TYPE_STRING, NULL);

    ax_event_key_value_set_add_key_value(
        key_value_set, "class_ids", NULL,
        class_ids_str->str, AX_VALUE_TYPE_STRING, NULL);

    // Create and send the event
    event = ax_event_new2(key_value_set, NULL);
    ax_event_handler_send_event(app_data->base.event_handler, app_data->base.event_id, event, NULL);

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    ax_event_free(event);

    // Free the GString objects
    g_string_free(class_labels_str, TRUE);
    g_string_free(class_ids_str, TRUE);

    // Returning TRUE keeps the timer going
    return TRUE;
}

/**
 * Send vehicle counting event with provided parameters
 */
gboolean send_event_counting(AppData_Counting *app_data, const gchar *vehicle_class,
                             gdouble speed, gint line, gint lane, const gchar *direction)
{
    AXEventKeyValueSet *key_value_set = NULL;
    AXEvent *event = NULL;

    // Update current data
    g_free(app_data->vehicle_class);
    app_data->vehicle_class = g_strdup(vehicle_class);
    app_data->speed = speed;
    app_data->line = line;
    app_data->lane = lane;

    g_free(app_data->direction);
    app_data->direction = g_strdup(!strcmp("down", direction) ? "FRONT" : "BACK");

    key_value_set = ax_event_key_value_set_new();

    // Add the event data to the set
    ax_event_key_value_set_add_key_value(
        key_value_set, "vehicle_class", NULL,
        app_data->vehicle_class, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "speed", NULL,
        &app_data->speed, AX_VALUE_TYPE_DOUBLE, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "line", NULL,
        &app_data->line, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "lane", NULL,
        &app_data->lane, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "direction", NULL,
        app_data->direction, AX_VALUE_TYPE_STRING, NULL);

    // Create and send the event
    event = ax_event_new2(key_value_set, NULL);
    ax_event_handler_send_event(app_data->base.event_handler, app_data->base.event_id, event, NULL);

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    ax_event_free(event);

    return TRUE;
}

/**
 * Send incident event with provided parameters
 */
gboolean send_event_incidents(AppData_Incidents *app_data, const gchar *vehicle_class,
                              const gchar *analytic_name, gint area_id,
                              gdouble speed, const gchar *filename)
{
    AXEventKeyValueSet *key_value_set = NULL;
    AXEvent *event = NULL;

    // Update current data
    g_free(app_data->vehicle_class);
    app_data->vehicle_class = g_strdup(vehicle_class);

    g_free(app_data->analytic_name);
    app_data->analytic_name = g_strdup(analytic_name);

    app_data->area_id = area_id;
    app_data->speed = speed;

    g_free(app_data->filename);
    app_data->filename = g_strdup(filename);

    key_value_set = ax_event_key_value_set_new();

    // Add the event data to the set
    ax_event_key_value_set_add_key_value(
        key_value_set, "vehicle_class", NULL,
        app_data->vehicle_class, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "analytic_name", NULL,
        app_data->analytic_name, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "area_id", NULL,
        &app_data->area_id, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "speed", NULL,
        &app_data->speed, AX_VALUE_TYPE_DOUBLE, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "filename", NULL,
        app_data->filename, AX_VALUE_TYPE_STRING, NULL);

    // Create and send the event
    event = ax_event_new2(key_value_set, NULL);
    ax_event_handler_send_event(app_data->base.event_handler, app_data->base.event_id, event, NULL);

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    ax_event_free(event);

    return TRUE;
}

/**
 * Declaration completion callback for stopline data
 */
void declaration_stopline_complete(guint declaration, gint *value)
{
    (void)declaration;
    // syslog(LOG_INFO, "StopLine declaration complete: %d", declaration);

    app_data_stopline->total_vehicles = *value;

    // Set up a timer to be called every 1 second
    app_data_stopline->base.timer = g_timeout_add_seconds(
        1, (GSourceFunc)send_event_stopline, app_data_stopline);
}

/**
 * Generic declaration complete callback
 */
void declaration_complete_callback(guint declaration, gint *value)
{
    (void)declaration;
    (void)value;
    // syslog(LOG_INFO, "Declaration complete for: %d", declaration);
}

/**
 * Generic function to set up event declaration with common parameters
 */
AXEventKeyValueSet *create_base_key_value_set(const gchar *topic1, guint token)
{
    AXEventKeyValueSet *key_value_set = ax_event_key_value_set_new();

    // Add common topic elements
    ax_event_key_value_set_add_key_value(
        key_value_set, "topic0", "tnsaxis",
        "CameraApplicationPlatform", AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "topic1", "tnsaxis",
        topic1, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "Token", NULL,
        &token, AX_VALUE_TYPE_INT, NULL);

    // Mark token properties
    ax_event_key_value_set_mark_as_source(key_value_set, "Token", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "Token", NULL,
        "wstype:tt:ReferenceToken", NULL);

    return key_value_set;
}

/**
 * Setup stopline event declaration including string fields for class data
 */
guint setup_stopline_declaration(AXEventHandler *event_handler)
{
    AXEventKeyValueSet *key_value_set = NULL;
    guint declaration = 0;
    gint start_value = 0;
    const gchar *empty_str = "";
    GError *error = NULL;

    // Create event structure
    key_value_set = create_base_key_value_set("EnixmaAnalytic_NumberOfDetections", 0);

    // Add data fields
    ax_event_key_value_set_add_key_value(
        key_value_set, "total_vehicles", NULL,
        &start_value, AX_VALUE_TYPE_INT, NULL);

    // Add string fields with empty initial values
    ax_event_key_value_set_add_key_value(
        key_value_set, "class_labels", NULL,
        empty_str, AX_VALUE_TYPE_STRING, NULL);

    ax_event_key_value_set_add_key_value(
        key_value_set, "class_ids", NULL,
        empty_str, AX_VALUE_TYPE_STRING, NULL);

    // Mark data properties
    ax_event_key_value_set_mark_as_data(key_value_set, "total_vehicles", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "total_vehicles", NULL,
        "wstype:xs:int", NULL);

    // Mark string properties
    ax_event_key_value_set_mark_as_data(key_value_set, "class_labels", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "class_labels", NULL,
        "wstype:xs:string", NULL);

    ax_event_key_value_set_mark_as_data(key_value_set, "class_ids", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "class_ids", NULL,
        "wstype:xs:string", NULL);

    // Declare event
    if (!ax_event_handler_declare(
            event_handler, key_value_set,
            FALSE, // Indicate a property state event
            &declaration,
            (AXDeclarationCompleteCallback)declaration_stopline_complete,
            &start_value,
            &error))
    {
        syslog(LOG_WARNING, "Could not declare stopline event: %s", error->message);
        g_error_free(error);
    }

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    return declaration;
}

/**
 * Setup counting event declaration
 */
guint setup_counting_declaration(AXEventHandler *event_handler)
{
    AXEventKeyValueSet *key_value_set = NULL;
    guint declaration = 0;
    gint start_value = 0;
    GError *error = NULL;
    const gchar *test_str = "TEST";
    gdouble start_double = 0.0;

    // Create event structure
    key_value_set = create_base_key_value_set("EnixmaAnalytic_Counting", 1);

    // Add data fields
    ax_event_key_value_set_add_key_value(
        key_value_set, "vehicle_class", NULL,
        test_str, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "speed", NULL,
        &start_double, AX_VALUE_TYPE_DOUBLE, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "line", NULL,
        &start_value, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "lane", NULL,
        &start_value, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "direction", NULL,
        test_str, AX_VALUE_TYPE_STRING, NULL);

    // Mark data properties
    ax_event_key_value_set_mark_as_data(key_value_set, "vehicle_class", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "vehicle_class", NULL,
        "wstype:xs:string", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "speed", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "speed", NULL,
        "wstype:xs:double", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "line", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "line", NULL,
        "wstype:xs:int", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "lane", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "lane", NULL,
        "wstype:xs:int", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "direction", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "direction", NULL,
        "wstype:xs:string", NULL);

    // Declare event
    if (!ax_event_handler_declare(
            event_handler, key_value_set,
            FALSE, // Indicate a property state event
            &declaration,
            (AXDeclarationCompleteCallback)declaration_complete_callback,
            &start_value,
            &error))
    {
        syslog(LOG_WARNING, "Could not declare counting event: %s", error->message);
        g_error_free(error);
    }

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    return declaration;
}

/**
 * Setup incidents event declaration
 */
guint setup_incidents_declaration(AXEventHandler *event_handler)
{
    AXEventKeyValueSet *key_value_set = NULL;
    guint declaration = 0;
    gint start_value = 0;
    GError *error = NULL;
    const gchar *test_str = "TEST";
    gdouble start_double = 0.0;

    // Create event structure
    key_value_set = create_base_key_value_set("EnixmaAnalytic_Incidents", 2);

    // Add data fields
    ax_event_key_value_set_add_key_value(
        key_value_set, "vehicle_class", NULL,
        test_str, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "analytic_name", NULL,
        test_str, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "area_id", NULL,
        &start_value, AX_VALUE_TYPE_INT, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "speed", NULL,
        &start_double, AX_VALUE_TYPE_DOUBLE, NULL);
    ax_event_key_value_set_add_key_value(
        key_value_set, "filename", NULL,
        test_str, AX_VALUE_TYPE_STRING, NULL);

    // Mark data properties
    ax_event_key_value_set_mark_as_data(key_value_set, "vehicle_class", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "vehicle_class", NULL,
        "wstype:xs:string", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "analytic_name", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "analytic_name", NULL,
        "wstype:xs:string", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "area_id", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "area_id", NULL,
        "wstype:xs:int", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "speed", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "speed", NULL,
        "wstype:xs:double", NULL);
    ax_event_key_value_set_mark_as_data(key_value_set, "filename", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(
        key_value_set, "filename", NULL,
        "wstype:xs:string", NULL);

    // Declare event
    if (!ax_event_handler_declare(
            event_handler, key_value_set,
            FALSE, // Indicate a property state event
            &declaration,
            (AXDeclarationCompleteCallback)declaration_complete_callback,
            &start_value,
            &error))
    {
        syslog(LOG_WARNING, "Could not declare incidents event: %s", error->message);
        g_error_free(error);
    }

    // Cleanup
    ax_event_key_value_set_free(key_value_set);
    return declaration;
}

/**
 * Free resources for a specific app data type
 */
void free_app_data(void *data, int type)
{
    if (!data)
        return;

    AppData_Base *base_data = (AppData_Base *)data;

    // Remove timer if active
    if (base_data->timer)
    {
        g_source_remove(base_data->timer);
        base_data->timer = 0;
    }

    // Clean up event handler
    if (base_data->event_id && base_data->event_handler)
    {
        ax_event_handler_undeclare(base_data->event_handler, base_data->event_id, NULL);
    }

    if (base_data->event_handler)
    {
        ax_event_handler_free(base_data->event_handler);
    }

    // Free specific data based on type
    switch (type)
    {
    case 1:
    { // StopLine
        // No additional fields to free
        break;
    }
    case 2:
    { // Counting
        AppData_Counting *counting = (AppData_Counting *)data;
        g_free(counting->vehicle_class);
        g_free(counting->direction);
        break;
    }
    case 3:
    { // Incidents
        AppData_Incidents *incidents = (AppData_Incidents *)data;
        g_free(incidents->vehicle_class);
        g_free(incidents->analytic_name);
        g_free(incidents->filename);
        break;
    }
    }

    free(data);
}