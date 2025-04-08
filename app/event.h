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
 * - send_event.h -
 *
 * Header file for the send_event application which demonstrates
 * how to send a stateful ONVIF event.
 */

#pragma once

#include <axsdk/axevent.h>
#include <glib-object.h>
#include <glib.h>

/*
 * Common data structure for all event types to reduce redundancy
 */
typedef struct {
    AXEventHandler *event_handler;
    guint event_id;
    guint timer;
} AppData_Base;

typedef struct {
    AppData_Base base;
    gint total_vehicles;
    gboolean incidents_area1;
    gboolean incidents_area2;
} AppData_StopLine;

typedef struct {
    AppData_Base base;
    gchar *vehicle_class;
    gdouble speed;
    gint line;
    gint lane;
    gchar *direction;
} AppData_Counting;

typedef struct {
    AppData_Base base;
    gchar *vehicle_class;
    gchar *analytic_name;
    gint area_id;
    gdouble speed;
    gchar *filename;
} AppData_Incidents;

// Global data structures
extern AppData_StopLine* app_data_stopline;
extern AppData_Counting *app_data_counting;
extern AppData_Incidents *app_data_incidents;

// Function declarations
gboolean send_event_stopline(AppData_StopLine *app_data);
gboolean send_event_counting(AppData_Counting *app_data, const gchar *vehicle_class, gdouble speed, gint line, gint lane, const gchar *direction);
gboolean send_event_incidents(AppData_Incidents *app_data, const gchar *vehicle_class, const gchar *analytic_name, gint area_id, gdouble speed, const gchar *filename);

void declaration_stopline_complete(guint declaration, gint *value);
void declaration_complete_callback(guint declaration, gint *value);

AXEventKeyValueSet* create_base_key_value_set(const gchar *topic1, guint token);

guint setup_stopline_declaration(AXEventHandler *event_handler);
guint setup_counting_declaration(AXEventHandler *event_handler);
guint setup_incidents_declaration(AXEventHandler *event_handler);

void free_app_data(void *data, int type);