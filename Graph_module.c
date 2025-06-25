#include "graph_module.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h> // For g_print

#define MAX_NODES 15  // A to O
static void add_connection(char a, char b);
typedef struct Node {
    char name;
    struct Node* neighbors[MAX_NODES];
    int neighbor_count;
} Node;
static Node nodes[MAX_NODES];
static bool graph_initialized = false;
static int get_index(char name) {
    return name - 'A';
}
void build_traffic_graph() {
    if (graph_initialized) {
        g_print("Graph already initialized.\n");
        return;
    }
    g_print("Initializing traffic graph with nodes A to O...\n");

    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].name = 'A' + i;
        nodes[i].neighbor_count = 0;
    }
    // Define connections (Spanning Tree from Image)
    add_connection('A', 'D');
    add_connection('D', 'C');
    add_connection('D', 'E');
    add_connection('D', 'I');
    add_connection('E', 'F');
    add_connection('E', 'J');
    add_connection('I', 'G');
    add_connection('I', 'K');
    add_connection('I', 'M');
    add_connection('F', 'B');
    add_connection('F', 'H');
    add_connection('J', 'L');
    add_connection('K', 'N');
    add_connection('L', 'O');

    graph_initialized = true;
    g_print("Traffic graph initialized with spanning tree connections.\n");
}
static void add_connection(char a, char b) {
    int index_a = get_index(a);
    int index_b = get_index(b);
    nodes[index_a].neighbors[nodes[index_a].neighbor_count++] = &nodes[index_b];
    nodes[index_b].neighbors[nodes[index_b].neighbor_count++] = &nodes[index_a];
    g_print("Connected %c <--> %c\n", a, b);
}

static bool find_path_dfs(Node* current, Node* target, char* path, int* path_len, bool* visited) {
    path[(*path_len)++] = current->name;
    visited[get_index(current->name)] = true;
    g_print("Visiting node %c\n", current->name);
    if (current == target) {
        g_print("Reached destination node %c\n", target->name);
        return true;
    }
    for (int i = 0; i < current->neighbor_count; i++) {
        Node* neighbor = current->neighbors[i];
        if (!visited[get_index(neighbor->name)]) {
            g_print("Exploring neighbor %c of %c\n", neighbor->name, current->name);
            if (find_path_dfs(neighbor, target, path, path_len, visited)) {
                return true;
            }
            (*path_len)--; // backtrack
            g_print("Backtracking from %c to %c\n", neighbor->name, current->name);
        }
    }
    return false;
}

char* getpath(char start, char end) {
    if (!graph_initialized) build_traffic_graph();
    int start_idx = get_index(start);
    int end_idx = get_index(end);
    if (start_idx < 0 || start_idx >= MAX_NODES || end_idx < 0 || end_idx >= MAX_NODES) {
        g_print("Invalid start or end node.\n");
        return NULL;
    }
    g_print("Finding path from %c to %c...\n", start, end);
    bool visited[MAX_NODES] = { false };
    char path[MAX_NODES];
    int path_len = 0;
    if (find_path_dfs(&nodes[start_idx], &nodes[end_idx], path, &path_len, visited)) {
        char* result = malloc(3 * path_len);
        int pos = 0;
        for (int i = 0; i < path_len; i++) {
            result[pos++] = path[i];
            if (i < path_len - 1) {
                result[pos++] = '-';
                result[pos++] = '>';
            }
        }
        result[pos] = '\0';
        g_print("Path found: %s\n", result);
        return result;
    }
    g_print("No path found from %c to %c\n", start, end);
    return NULL;
}
void cleanup_traffic_graph() {
    graph_initialized = false;
    g_print("Traffic graph cleaned up.\n");
}
