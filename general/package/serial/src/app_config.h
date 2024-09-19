#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct AppConfig
{
    // [serial]
    char port[128];
    int baudrate;
    int package_size;
};

extern struct AppConfig app_config;

enum ConfigError parse_app_config(void);
void restore_app_config(void);
int save_app_config(void);
