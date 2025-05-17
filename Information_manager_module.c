#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
typedef struct {
    const char *vehicle_type;
    const char *source;
    const char *destination;
    int id;
    char **path;
    int elapsed;
    int total;
    int active;
} Vehicle_info_struct;
#define MAX_VEHICLES 100
#define MAX_LINE_LEN 256
Vehicle_info_struct *all_vehicles[MAX_VEHICLES];
int vehicle_count = 0;
pthread_mutex_t mutex;
void *vehicle_runner(void *arg) {
    Vehicle_info_struct *v = (Vehicle_info_struct *)arg;
    while (v->elapsed < v->total) {
        sleep(1);
        pthread_mutex_lock(&mutex);
        v->elapsed++;
        pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_lock(&mutex);
    v->active = 0;
    pthread_mutex_unlock(&mutex);
    return NULL;
}
void *display_loop(void *arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&mutex);
        int active_count = 0;
        for (int i = 0; i < vehicle_count; i++) {
            if (all_vehicles[i]->active)
                active_count++;
        }
        char **arr = malloc(sizeof(char *) * (active_count + 2)); // +2 for header and NULL terminator
        arr[0] = malloc(MAX_LINE_LEN);
        snprintf(arr[0], MAX_LINE_LEN, "ID\tVehicle Type\tSource\tDestination\tElapsed(s)\tTotal(s)\n");
        int index = 1;
        for (int i = 0; i < vehicle_count; i++) {
            Vehicle_info_struct *v = all_vehicles[i];
            if (v->active) {
                arr[index] = malloc(MAX_LINE_LEN);
                snprintf(arr[index], MAX_LINE_LEN, "%d\t%-12s\t%s\t\t%s\t\t%d\t\t%d",
                         v->id, v->vehicle_type, v->source, v->destination, v->elapsed, v->total);
                index++;
            }
        }
        arr[index] = NULL; // End marker for display_multiple_lines
        pthread_mutex_unlock(&mutex);
        display_multiple_lines(arr);
        // Free memory
        for (int i = 0; i < index; i++) {
            free(arr[i]);
        }
        free(arr);
    }
}
void addVehicleInformation(Vehicle_info_struct vis) {
    pthread_mutex_lock(&mutex);
    Vehicle_info_struct *v = malloc(sizeof(Vehicle_info_struct));
    *v = vis;
    v->elapsed = 0;
    v->total = total_time_between_nodes(v->path);
    v->active = 1;
    all_vehicles[vehicle_count++] = v;
    pthread_t t;
    pthread_create(&t, NULL, vehicle_runner, v);
    pthread_detach(t);
    pthread_mutex_unlock(&mutex);
}
