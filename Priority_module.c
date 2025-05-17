#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vehicle.h"
int is_id_generated(int id, int* generated_ids, int num_generated) {
    for (int i = 0; i < num_generated; i++) {
        if (generated_ids[i] == id) {
            return 1; // ID has been generated
        }
    }
    return 0; // ID has not been generated
}
int generate_random_id(char* vehicle_type, int* generated_ids, int num_generated) {
    int id;
    do {
        if (strcmp(vehicle_type, "Ambulance") == 0) {
            id = 100 + rand() % 100; // Generate ID between 100 and 199
        } else if (strcmp(vehicle_type, "Truck") == 0) {
            id = 200 + rand() % 100; // Generate ID between 200 and 299
        } else if (strcmp(vehicle_type, "Car") == 0) {
            id = 300 + rand() % 100; // Generate ID between 300 and 399
        } else if (strcmp(vehicle_type, "Bike") == 0) {
            id = 400 + rand() % 100; // Generate ID between 400 and 499
        } else {
            printf("Invalid vehicle type\n");
            return -1;
        }
    } while (is_id_generated(id, generated_ids, num_generated));
    return id;
}
int set_priority(int vehicle_id) {
    if (vehicle_id >= 100 && vehicle_id <= 199) {
        return 1; // Ambulance
    } else if (vehicle_id >= 200 && vehicle_id <= 299) {
        return 2; // Truck
    } else if (vehicle_id >= 300 && vehicle_id <= 399) {
        return 3; // Car
    } else if (vehicle_id >= 400 && vehicle_id <= 499) {
        return 4; // Bike
    } else {
        printf("Invalid vehicle ID\n");
        return -1;
    }
}
int * priority_module(char * vehicle_type, int* generated_ids, int num_generated) {
    static int a[2];
    a[0]=generate_random_id(vehicle_type, generated_ids, num_generated);
    a[1]=set_priority(a[0]);
    return a;
}
