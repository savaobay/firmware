#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

// Snapshot result structure
typedef struct {
    bool success;
    char filepath[256];
    char error_message[128];
} SnapshotResult;

// Initialize snapshot functionality and start thread
int start_snapshot_service(void);

// Stop snapshot service and cleanup resources
void stop_snapshot_service(void);

// Take a single snapshot and save to file
SnapshotResult take_snapshot(void);

// Set/Get snapshot interval (in seconds)
void set_snapshot_interval(int seconds);
int get_snapshot_interval(void);

#endif // SNAPSHOT_H