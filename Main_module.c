#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "interface_module.h"
#define MAX_EDGES 100
#define MAX_LINE 10000
#define MAX_PATHS 1000
#define MAX_PIXELS 10000
#define MAX_TEXT_LENGTH 2048

typedef struct {
    char from;
    char to;
    int weight;
} Edge;

typedef struct {
    char node1, node2;
    int **pixels;
    int pixel_count;
} Path;

typedef struct {
    char source;        
    char destination;   
    int id;
    int priority;
} graph_pathsetter;

typedef struct {
    const char *vehicle_type;
    const char *source;
    const char *destination;
} Vehicle;

typedef struct {
    const char *vehicle_type;
    const char *source;
    const char *destination;
    int id;
    char **path;
} Vehicle_info_struct;

Edge edges[MAX_EDGES];
int edgeCount = 0;

// Function declarations (assuming they're defined in interface_module.h)
int *set_priority_module(const char *vehicle_type);
char **getpath(graph_pathsetter gp);
void addVehicleInformation(Vehicle_info_struct vis);
void display_traffic_info(const char *path);

// Function to add a new vehicle
void add_new_vehicle(Vehicle vr) {
    if (!vr.vehicle_type || !vr.source || !vr.destination) {
        fprintf(stderr, "Error: Invalid vehicle information\n");
        return;
    }

    int *priority_module_output = set_priority_module(vr.vehicle_type);
    if (!priority_module_output) {
        fprintf(stderr, "Error: Failed to get priority information\n");
        return;
    }

    graph_pathsetter gp;
    gp.id = priority_module_output[0];
    gp.priority = priority_module_output[1];
    gp.destination = *vr.destination;
    gp.source = *vr.source;

    char **path = getpath(gp);
    if (!path) {
        fprintf(stderr, "Error: Failed to get path\n");
        return;
    }

    Vehicle_info_struct vis;
    vis.source = vr.source;
    vis.destination = vr.destination;
    vis.id = gp.id;
    vis.vehicle_type = vr.vehicle_type;
    vis.path = path;

    addVehicleInformation(vis);
}

// Function to display multiple lines of text
void display_multiple_lines(char *arr[]) {
    if (!arr) return;

    char full_text[MAX_TEXT_LENGTH] = "";
    size_t remaining_space = MAX_TEXT_LENGTH - 1;

    for (int i = 0; arr[i] != NULL && remaining_space > 0; i++) {
        size_t len = strlen(arr[i]);
        if (len >= remaining_space) {
            strncat(full_text, arr[i], remaining_space);
            break;
        }
        strcat(full_text, arr[i]);
        strcat(full_text, "\n");
        remaining_space -= (len + 1);
    }

    display_traffic_info(full_text);
}

// Function to calculate total time between nodes
int total_time_between_nodes(char *path[]) {
    if (!path || !path[0]) return -1;

    FILE *file = fopen("node_time.txt", "r");
    if (!file) {
        fprintf(stderr, "Error opening file: node_time.txt\n");
        return -1;
    }
    edgeCount = 0;
    while (edgeCount < MAX_EDGES && 
           fscanf(file, " %c %c %d", &edges[edgeCount].from, &edges[edgeCount].to, &edges[edgeCount].weight) == 3) {
        edgeCount++;
    }
    fclose(file);
    int totalWeight = 0;
    for (int i = 0; path[i] != NULL && path[i+1] != NULL; i++) {
        char from = path[i][0];
        char to = path[i + 1][0];
        int found = 0;
        for (int j = 0; j < edgeCount; j++) {
            if ((edges[j].from == from && edges[j].to == to) || 
                (edges[j].from == to && edges[j].to == from)) {
                totalWeight += edges[j].weight;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Warning: No edge found between %c and %c\n", from, to);
        }
    }

    return totalWeight;
}

// Function to get pixels between nodes
int** pixels_between_nodes(char *path[], int *num_pixels) {
    if (!path || !path[0] || !num_pixels) {
        return NULL;
    }

    FILE *file = fopen("node_path.txt", "r");
    if (!file) {
        fprintf(stderr, "Error opening file: node_path.txt\n");
        return NULL;
    }

    Path paths[MAX_PATHS];
    int path_count = 0;
    char line[MAX_LINE];

    while (path_count < MAX_PATHS && fgets(line, sizeof(line), file)) {
        char n1, n2;
        if (sscanf(line, " %c %c", &n1, &n2) != 2) {
            continue;
        }

        Path *p = &paths[path_count];
        p->node1 = n1;
        p->node2 = n2;
        p->pixel_count = 0;
        p->pixels = malloc(MAX_PIXELS * sizeof(int*));
        if (!p->pixels) {
            fprintf(stderr, "Memory allocation failed\n");
            continue;
        }
        if (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, " ");
            while (token && p->pixel_count < MAX_PIXELS) {
                int x, y;
                if (sscanf(token, "%d,%d", &x, &y) == 2) {
                    p->pixels[p->pixel_count] = malloc(2 * sizeof(int));
                    if (!p->pixels[p->pixel_count]) {
                        fprintf(stderr, "Memory allocation failed\n");
                        token = strtok(NULL, " ");
                        continue;
                    }
                    p->pixels[p->pixel_count][0] = x;
                    p->pixels[p->pixel_count][1] = y;
                    p->pixel_count++;
                }
                token = strtok(NULL, " ");
            }
        }
        path_count++;
    }
    fclose(file);

    int **result = malloc(MAX_PIXELS * sizeof(int*));
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    int result_count = 0;
    for (int i = 0; path[i + 1] != NULL && result_count < MAX_PIXELS; i++) {
        char from = path[i][0];
        char to = path[i + 1][0];
        int found = 0;

        for (int j = 0; j < path_count && !found; j++) {
            if ((paths[j].node1 == from && paths[j].node2 == to) ||
                (paths[j].node1 == to && paths[j].node2 == from)) {
                found = 1;
                int direction = (paths[j].node1 == from) ? 1 : -1;
                int start = (direction == 1) ? 0 : paths[j].pixel_count - 1;
                int end = (direction == 1) ? paths[j].pixel_count : -1;

                for (int k = start; k != end && result_count < MAX_PIXELS; k += direction) {
                    result[result_count] = malloc(2 * sizeof(int));
                    if (!result[result_count]) {
                        fprintf(stderr, "Memory allocation failed\n");
                        continue;
                    }
                    result[result_count][0] = paths[j].pixels[k][0];
                    result[result_count][1] = paths[j].pixels[k][1];
                    result_count++;
                }
            }
        }

        if (!found) {
            fprintf(stderr, "Warning: No path found between %c and %c\n", from, to);
        }
    }
    // Clean up allocated memory for paths
    for (int i = 0; i < path_count; i++) {
        for (int j = 0; j < paths[i].pixel_count; j++) {
            free(paths[i].pixels[j]);
        }
        free(paths[i].pixels);
    }

    *num_pixels = result_count;
    return result;
}
