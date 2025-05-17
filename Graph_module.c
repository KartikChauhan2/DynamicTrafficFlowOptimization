#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_NODES 15  // A-O (15 nodes)

typedef struct Node {
    char name;
    struct Node* neighbors[MAX_NODES];
    int neighbor_count;
} Node;

static Node nodes[MAX_NODES];
static bool graph_initialized = false;

// Helper function to get node index
static int get_index(char name) {
    return name - 'A';
}

void build_traffic_graph() {
    if (graph_initialized) return;
    
    // Initialize all nodes
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].name = 'A' + i;
        nodes[i].neighbor_count = 0;
    }

    // Build connections (A-O traffic network)
    // Main horizontal route: A-B-C-D-E-F
    add_connection('A', 'B');
    add_connection('B', 'C');
    add_connection('C', 'D');
    add_connection('D', 'E');
    add_connection('E', 'F');
    
    // Vertical routes
    add_connection('A', 'G');
    add_connection('G', 'H');
    add_connection('H', 'I');
    
    add_connection('A', 'J');
    add_connection('J', 'K');
    add_connection('K', 'L');
    add_connection('L', 'M');
    
    add_connection('A', 'N');
    add_connection('N', 'O');
    
    graph_initialized = true;
}

// Internal connection helper
static void add_connection(char a, char b) {
    int index_a = get_index(a);
    int index_b = get_index(b);
    
    nodes[index_a].neighbors[nodes[index_a].neighbor_count++] = &nodes[index_b];
    nodes[index_b].neighbors[nodes[index_b].neighbor_count++] = &nodes[index_a];
}

// DFS path finding helper
static bool find_path_dfs(Node* current, Node* target, char* path, int* path_len, bool* visited) {
    path[(*path_len)++] = current->name;
    visited[get_index(current->name)] = true;
    if (current == target) return true;
    for (int i = 0; i < current->neighbor_count; i++) {
        Node* neighbor = current->neighbors[i];
        if (!visited[get_index(neighbor->name)]) {
            if (find_path_dfs(neighbor, target, path, path_len, visited)) {
                return true;
            }
            (*path_len)--; // backtrack
        }
    }
    return false;
}
char* find_traffic_path(char start, char end) {
    if (!graph_initialized) build_traffic_graph();
    int start_idx = get_index(start);
    int end_idx = get_index(end);
    if (start_idx < 0 || end_idx < 0) return NULL;
    bool visited[MAX_NODES] = {false};
    char path[MAX_NODES];
    int path_len = 0;
    if (find_path_dfs(&nodes[start_idx], &nodes[end_idx], path, &path_len, visited)) {
        // Format path string
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
        return result;
    }
    return NULL;
}
void cleanup_traffic_graph() {
    // No dynamic allocation in this version
    graph_initialized = false;
}
