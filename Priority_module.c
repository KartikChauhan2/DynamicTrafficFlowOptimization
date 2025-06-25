#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>  // For g_print
#include "priority_manager_module.h"

// Check if an ID is already generated
int is_id_generated(int id, int* generated_ids, int num_generated) {
    for (int i = 0; i < num_generated; i++) {
        if (generated_ids[i] == id) {
            return 1; // Already generated
        }
    }
    return 0;
}

// Generate random ID based on vehicle type
int generate_random_id(char* vehicle_type, int* generated_ids, int num_generated) {
    int id;
    do {
        if (strcmp(vehicle_type, "Ambulance") == 0) {
            id = 100 + rand() % 100;
        } else if (strcmp(vehicle_type, "Truck") == 0) {
            id = 200 + rand() % 100;
        } else if (strcmp(vehicle_type, "Car") == 0) {
            id = 300 + rand() % 100;
        } else if (strcmp(vehicle_type, "Bike") == 0) {
            id = 400 + rand() % 100;
        } else {
            g_print("Invalid vehicle type: %s\n", vehicle_type);
            return -1;
        }
    } while (is_id_generated(id, generated_ids, num_generated));

    g_print("Generated new ID %d for vehicle type: %s\n", id, vehicle_type);
    return id;
}

// Determine priority from vehicle ID
int set_priority(int vehicle_id) {
    if (vehicle_id >= 100 && vehicle_id <= 199) return 1;     // Ambulance
    if (vehicle_id >= 200 && vehicle_id <= 299) return 2;     // Truck
    if (vehicle_id >= 300 && vehicle_id <= 399) return 3;     // Car
    if (vehicle_id >= 400 && vehicle_id <= 499) return 4;     // Bike

    g_print("Invalid vehicle ID: %d\n", vehicle_id);
    return -1;
}

// Main priority module that returns ID and priority
int * priority_module(char * vehicle_type, int* generated_ids, int num_generated) {
    static int a[2];
    a[0] = generate_random_id(vehicle_type, generated_ids, num_generated);
    if (a[0] == -1) {
        g_print("Error: ID generation failed for vehicle type: %s\n", vehicle_type);
        a[1] = -1;
        return a;
    }

    a[1] = set_priority(a[0]);
    g_print("Vehicle Type: %s | ID: %d | Priority: %d\n", vehicle_type, a[0], a[1]);
    return a;
}

// Set priority module (used externally)
int *set_priority_module(const char *vehicle_type) {
    static int generated_ids[1000];
    static int num_generated = 0;
    int *result = priority_module((char *)vehicle_type, generated_ids, num_generated);

    if (result && result[0] != -1) {
        generated_ids[num_generated++] = result[0];
        g_print("set_priority_module(): Stored generated ID: %d\n", result[0]);
    } else {
        g_print("set_priority_module(): Failed to assign ID for vehicle type: %s\n", vehicle_type);
    }

    return result;
}
