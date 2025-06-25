#include "main_module.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "priority_manager_module.h"
#include "graph_module.h"
#include <gtk/gtk.h>
#define MAX_EDGES 100

Edge edges[MAX_EDGES];
int edgeCount = 0;

void addVehicleInformation(Vehicle_info_struct *vis);
void display_traffic_info(const char *path);
void (*animate_path_callback)(int (*pixels)[2], int length) = NULL;

static void interpolate_pixels(int x1, int y1, int x2, int y2, int (*output)[2], int *count) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        output[*count][0] = x1;
        output[*count][1] = y1;
        (*count)++;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

int **pixels_between_nodes(char *path[]) {
    if (!path || !path[0]) {
        g_print("pixels_between_nodes: Path is null or empty\n");
        return NULL;
    }
    FILE *file = fopen("path.txt", "r");
    if (!file) {
        g_print("Error opening file: path.txt\n");
        return NULL;
    }

    typedef struct { char start[10]; char end[10]; int pixels[MAX_PIXELS][2]; int length; } ParsedPath;
    ParsedPath paths[MAX_PATHS];
    int pathCount = 0;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, " ");
        if (!token) continue;
        ParsedPath p; strcpy(p.start, token);
        token = strtok(NULL, " ");
        if (!token) continue; strcpy(p.end, token);
        p.length = 0;
        while ((token = strtok(NULL, " ,\n")) != NULL && p.length < MAX_PIXELS) {
            int x = atoi(token);
            token = strtok(NULL, " ,\n");
            if (!token) break;
            int y = atoi(token);
            p.pixels[p.length][0] = x;
            p.pixels[p.length][1] = y;
            p.length++;
        }
        if (pathCount < MAX_PATHS) paths[pathCount++] = p;
    }
    fclose(file);

    int **finalPixels = malloc(100 * sizeof(int *));
    int finalCount = 0, finalCapacity = 100;

    if (!finalPixels) {
        g_print("Memory allocation failed for finalPixels\n");
        return NULL;
    }

    for (int i = 0; path[i + 1] != NULL; i++) {
        char *src = path[i];
        char *dst = path[i + 1];
        int found = 0;
        for (int j = 0; j < pathCount && !found; j++) {
            int forward = strcmp(paths[j].start, src) == 0 && strcmp(paths[j].end, dst) == 0;
            int reverse = strcmp(paths[j].start, dst) == 0 && strcmp(paths[j].end, src) == 0;
            if (forward || reverse) {
                found = 1;
                for (int k = 0; k < paths[j].length - 1; k++) {
                    int idx1 = forward ? k : paths[j].length - 1 - k;
                    int idx2 = forward ? k + 1 : paths[j].length - 2 - k;
                    int x1 = paths[j].pixels[idx1][0];
                    int y1 = paths[j].pixels[idx1][1];
                    int x2 = paths[j].pixels[idx2][0];
                    int y2 = paths[j].pixels[idx2][1];
                    int tempCount = 0;
                    int tempPixels[100][2];
                    interpolate_pixels(x1, y1, x2, y2, tempPixels, &tempCount);
                    for (int m = 0; m < tempCount; m++) {
                        if (finalCount >= finalCapacity) {
                            finalCapacity *= 2;
                            finalPixels = realloc(finalPixels, finalCapacity * sizeof(int *));
                            if (!finalPixels) {
                                g_print("Memory reallocation failed\n");
                                return NULL;
                            }
                        }
                        finalPixels[finalCount] = malloc(2 * sizeof(int));
                        finalPixels[finalCount][0] = tempPixels[m][0];
                        finalPixels[finalCount][1] = tempPixels[m][1];
                        finalCount++;
                    }
                }
            }
        }
        if (!found) g_print("Warning: No path found between %s and %s\n", src, dst);
    }

    finalPixels[finalCount] = NULL;
    return finalPixels;
}

int **pixels_of_nodes(char *source, char *destination) {
    FILE *file = fopen("node_pixels.txt", "r");
    if (!file) {
        g_print("Error opening file: node_pixels.txt\n");
        return NULL;
    }

    int **pixels = malloc(2 * sizeof(int *));
    pixels[0] = pixels[1] = NULL;
    char line[100];

    while (fgets(line, sizeof(line), file)) {
        char node[10]; int x, y;
        if (sscanf(line, "%s %d,%d", node, &x, &y) == 3) {
            if (strcmp(node, source) == 0) {
                pixels[0] = malloc(2 * sizeof(int));
                pixels[0][0] = x; pixels[0][1] = y;
            }
            if (strcmp(node, destination) == 0) {
                pixels[1] = malloc(2 * sizeof(int));
                pixels[1][0] = x; pixels[1][1] = y;
            }
        }
        if (pixels[0] && pixels[1]) break;
    }
    fclose(file);

    if (!pixels[0] || !pixels[1]) {
        g_print("Error: One or both nodes not found in node_pixels.txt\n");
        if (pixels[0]) free(pixels[0]);
        if (pixels[1]) free(pixels[1]);
        free(pixels);
        return NULL;
    }
    return pixels;
}


static char **parse_path_to_array(const char *dfs_path) {
    if (!dfs_path) {
        g_print("Error: dfs_path is NULL\n");
        return NULL;
    }
    int len = strlen(dfs_path);
    char **result = malloc((len + 2) * sizeof(char *));
    int idx = 0;
    char *copy = strdup(dfs_path);
    char *token = strtok(copy, "->");

    while (token) {
        result[idx] = malloc(2);
        result[idx][0] = token[0];
        result[idx][1] = '\0';
        idx++;
        token = strtok(NULL, "->");
    }
    result[idx] = NULL;
    free(copy);
    return result;
}

void add_new_vehicle(Vehicle vr) {
    g_print("add_new_vehicle called\n");

    if (!vr.vehicle_type || !vr.source || !vr.destination) {
        g_print("Error: Invalid vehicle information\n");
        return;
    }

    int *priority_module_output = set_priority_module(vr.vehicle_type);
    if (!priority_module_output || priority_module_output[0] == -1 || priority_module_output[1] == -1) {
        g_print("Error: Failed to get priority or ID information\n");
        return;
    }

    int assigned_id = priority_module_output[0];
    int assigned_priority = priority_module_output[1];

    g_print("Assigned ID: %d | Priority: %d\n", assigned_id, assigned_priority);

    // Get DFS path between source and destination
    char *dfs_path = getpath(*vr.source, *vr.destination);
    char **path = NULL;
    if (dfs_path) {
        g_print("DFS Path found: %s\n", dfs_path);
        path = parse_path_to_array(dfs_path);
        free(dfs_path);
    }
    if (!path) {
        g_print("Error: Failed to get path\n");
        return;
    }

    // Convert node path to pixel path
    int **pixels = pixels_between_nodes(path);
    int length = 0;
    while (pixels && pixels[length]) length++;

    int (*converted_path)[2] = malloc(sizeof(int[2]) * length);
    for (int i = 0; i < length; i++) {
        converted_path[i][0] = pixels[i][0];
        converted_path[i][1] = pixels[i][1];
        free(pixels[i]);
    }
    free(pixels);

    // Animate if callback is set
    if (animate_path_callback && length > 0) {
        g_print("Animating path of length %d...\n", length);
        animate_path_callback(converted_path, length);
    } else {
        g_print("No animation callback set or path length is zero\n");
    }
    free(converted_path);

    // Fill vehicle info structure
    Vehicle_info_struct vis;
    vis.id = assigned_id;
    vis.vehicle_type = vr.vehicle_type;
    vis.source = vr.source;
    vis.destination = vr.destination;
    vis.path = path;

    // Add to the simulation
    addVehicleInformation(&vis);

    // Cleanup dynamic path array
    for (int i = 0; path[i]; ++i) free(path[i]);
    free(path);
}


int total_time_between_nodes(char *path[]) {
    if (!path || !path[0]) {
        g_print("Error: Path is null or empty\n");
        return -1;
    }

    FILE *file = fopen("node_time.txt", "r");
    if (!file) {
        g_print("Error opening file: node_time.txt\n");
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
            g_print("Warning: No edge found between %c and %c\n", from, to);
        }
    }

    return totalWeight;
}
