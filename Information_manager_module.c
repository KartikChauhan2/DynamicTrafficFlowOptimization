#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>
#include "main_module.h"
#include "interface_module.h"  // <- includes display_traffic_info directly

pthread_mutex_t mutex;
Vehicle_info_struct *all_vehicles[MAX_VEHICLES];
int vehicle_count = 0;

// --- Worker Thread: Each Vehicle's Timer ---
void *vehicle_runner(void *arg) {
    Vehicle_info_struct *v = (Vehicle_info_struct *)arg;
    g_print("Vehicle ID %d started. Tracking elapsed time...\n", v->id);

    while (v->elapsed < v->total) {
        sleep(1);
        pthread_mutex_lock(&mutex);
        v->elapsed++;
        pthread_mutex_unlock(&mutex);

        g_print("Vehicle ID %d | Elapsed: %d / %d\n", v->id, v->elapsed, v->total);
    }

    pthread_mutex_lock(&mutex);
    v->active = 0;
    pthread_mutex_unlock(&mutex);

    g_print("Vehicle ID %d finished its journey.\n", v->id);
    return NULL;
}

// --- UI Update (Called from Main Thread) ---
gboolean safe_display_multiple_lines(gpointer data) {
    char **arr = (char **)data;
    if (!arr) return G_SOURCE_REMOVE;

    char full_text[MAX_TEXT_LENGTH] = "";
    size_t remaining = MAX_TEXT_LENGTH - 1;

    for (int i = 0; arr[i] && remaining > 0; i++) {
        size_t len = strlen(arr[i]);
        if (len >= remaining) {
            strncat(full_text, arr[i], remaining);
            break;
        }
        strcat(full_text, arr[i]);
        strcat(full_text, "\n");
        remaining -= (len + 1);
    }

    g_print("========== Display Traffic Called ==========\n%s\n===========================================\n", full_text);
    display_traffic_info(full_text);  // ← updates GtkTextView

    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);

    return G_SOURCE_REMOVE;
}

// --- UI Clear ---
gboolean safe_display_clear(gpointer data) {
    display_traffic_info("");
    return G_SOURCE_REMOVE;
}

int priority(const char* veh)
{
    if (strcmp(veh, "Ambulance") == 0) {
            return 4;
        } 
    if (strcmp(veh, "Truck") == 0) {
            return 3;
        }
    if (strcmp(veh, "Car") == 0) {
            return 2;
    }
    if (strcmp(veh, "Bike") == 0) {
            return 1;
     }
}
// --- Display Thread: Monitors Active Vehicles ---
void *display_loop(void *arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&mutex);

        int active_count = 0;
        for (int i = 0; i < vehicle_count; i++) {
            if (all_vehicles[i]->active)
                active_count++;
        }

        if (active_count > 0) {
            char **arr = malloc(sizeof(char *) * (active_count + 2));
            arr[0] = malloc(MAX_LINE_LEN);
            snprintf(arr[0], MAX_LINE_LEN, "ID\tVehicle Type\tPriority\tSource\tDestination\tElapsed(s)\tTotal(s)");

            int index = 1;
            for (int i = 0; i < vehicle_count; i++) {
                Vehicle_info_struct *v = all_vehicles[i];
                if (v->active) {
                    int pri=priority(v->vehicle_type);
                    arr[index] = malloc(MAX_LINE_LEN);
                    snprintf(arr[index], MAX_LINE_LEN, "%d\t%-12s\t%d\t\t%s\t\t%s\t\t%d\t\t%d",
                             v->id, v->vehicle_type,pri, v->source, v->destination, v->elapsed, v->total);
                    index++;
                }
            }
            arr[index] = NULL;
            pthread_mutex_unlock(&mutex);

            g_main_context_invoke(NULL, safe_display_multiple_lines, arr);
        } else {
            pthread_mutex_unlock(&mutex);
            g_main_context_invoke(NULL, safe_display_clear, NULL);
        }
    }
}


// --- Adds a New Vehicle and Starts Timer Thread ---
void addVehicleInformation(Vehicle_info_struct *vis) {
    pthread_mutex_lock(&mutex);

    Vehicle_info_struct *v = malloc(sizeof(Vehicle_info_struct));
    *v = *vis;
    v->elapsed = 0;
    v->total = total_time_between_nodes(v->path);  // Assume implemented
    v->active = 1;
    
    all_vehicles[vehicle_count++] = v;

    g_print("Vehicle added: ID=%d, Type=%s, From=%s, To=%s, Total Time=%d sec\n",
            v->id, v->vehicle_type, v->source, v->destination, v->total);

    pthread_t t;
    pthread_create(&t, NULL, vehicle_runner, v);
    pthread_detach(t);

    pthread_mutex_unlock(&mutex);
}

// --- Starts the Background Display Thread ---
void start_display_loop() {
    pthread_t display_thread;
    pthread_create(&display_thread, NULL, display_loop, NULL);
    pthread_detach(display_thread);
}
