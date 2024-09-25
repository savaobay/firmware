#include "common.h"
#include "region.h"
#include <signal.h>
extern volatile sig_atomic_t keep_running;
int start_serial_handler();
void stop_serial_handler();
