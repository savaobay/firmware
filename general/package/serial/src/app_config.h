#pragma once

#include "config.h"

struct AppConfig
{
    // [serial]
    char port[128];
    int baudrate;
};

extern struct AppConfig app_config;

enum ConfigError parse_app_config(const char *path);
