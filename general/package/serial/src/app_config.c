#include "app_config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AppConfig app_config;

// Function to update or add a key-value pair in a specific section of an ini
// file
enum ConfigError write_config(const char *path, const char *section, const char *key, const char *value)
{
    FILE *file = fopen(path, "r+");
    if (!file)
    {
        perror("write_config: Unable to open config file for reading");
        return -1;
    }

    // Read the entire file content into memory
    fseek(file, 0, SEEK_END);
    size_t length = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(length + 1);
    if (!content)
    {
        fclose(file);
        return -1;
    }
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

    // Search for the section
    char *section_start = strstr(content, section);
    if (!section_start)
    {
        // If the section is not found, append it to the file
        section_start = content + length;
        sprintf(section_start, "\n[%s]\n", section);
    }

    // Search for the key in the section
    char *key_start = strstr(section_start, key);
    if (!key_start)
    {
        // If the key is not found, append it to the section
        key_start = section_start + strlen(section_start);
        sprintf(key_start, "%s=%s\n", key, value);
    }
    else
    {
        // If the key is found, replace its value
        char *line_end = strchr(key_start, '\n');
        if (!line_end)
            line_end = key_start + strlen(key_start);
        char *value_start = strchr(key_start, '=');
        if (value_start && value_start < line_end)
        {
            value_start++;
            while (*value_start == ' ')
                value_start++; // Skip spaces
            strcpy(value_start, value);
            strcpy(value_start + strlen(value), line_end);
        }
    }

    // Write the updated content back to the file
    file = fopen(path, "w");
    if (!file)
    {
        free(content);
        perror("write_config: Unable to open config file for writing");
        return CONFIG_ERR_WRITE;
    }
    fwrite(content, 1, strlen(content), file);
    fclose(file);

    free(content);
    return CONFIG_OK;
}

enum ConfigError parse_app_config(const char *path)
{
    memset(&app_config, 0, sizeof(struct AppConfig));

    app_config.port[0] = 0;
    app_config.baudrate = 115200;

    struct IniConfig ini;
    memset(&ini, 0, sizeof(struct IniConfig));

    // load config file to string
    ini.str = NULL;
    {
        char config_path[50];
        FILE *file = fopen("./serial.ini", "rb");
        if (!file)
        {
            file = fopen("/etc/serial.ini", "rb");
            if (!file)
            {
                printf("Can't find config serial.ini in:\n"
                       "    ./serial.ini\n    /etc/serial.ini\n");
                return -1;
            }
        }

        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        ini.str = malloc(length + 1);
        if (!ini.str)
        {
            printf("Can't allocate buf in parse_app_config\n");
            fclose(file);
            return -1;
        }
        size_t n = fread(ini.str, 1, length, file);
        if (n != length)
        {
            printf("Can't read all file %s\n", path);
            fclose(file);
            free(ini.str);
            return -1;
        }
        fclose(file);
        ini.str[length] = 0;
    }

    enum ConfigError err;
    find_sections(&ini);

    err = parse_param_value(&ini, "serial", "port", app_config.port);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(&ini, "serial", "baudrate", 0, 115200, &app_config.baudrate);
    if (err != CONFIG_OK)
        goto RET_ERR;

    free(ini.str);
    return CONFIG_OK;
RET_ERR:
    free(ini.str);
    return err;
}
