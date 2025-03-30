#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdbool.h>
#include <signal.h>

extern volatile sig_atomic_t keep_running;
void *start_storage_handle(void);
void stop_storage_handle(void);

#endif // STORAGE_SERVICE_H
