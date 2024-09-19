#include "app_config.h"
const char *appconf_paths[] = {"./serial.yaml", "/etc/serial.yaml"};

struct AppConfig app_config;

static inline void *open_app_config(FILE **file, const char *flags)
{
    const char **path = appconf_paths;
    *file = NULL;

    while (*path)
    {
        if (access(*path++, F_OK))
            continue;
        if (*flags == 'w')
        {
            char bkPath[32];
            sprintf(bkPath, "%s.bak", *(path - 1));
            remove(bkPath);
            rename(*(path - 1), bkPath);
        }
        *file = fopen(*(path - 1), flags);
        break;
    }
}

void restore_app_config(void)
{
    const char **path = appconf_paths;

    while (*path)
    {
        char bkPath[32];
        sprintf(bkPath, "%s.bak", *path);
        if (!access(bkPath, F_OK))
        {
            remove(*path);
            rename(bkPath, *path);
        }
        path++;
    }
}

int save_app_config(void)
{
    FILE *file;

    open_app_config(&file, "w");
    if (!file)
        fprintf(stderr, "Can't open config file for writing\n");

    fprintf(file, "serial:\n");
    fprintf(file, "  port: %s\n", app_config.port);
    fprintf(file, "  baudrate: %d\n", app_config.baudrate);
    fprintf(file, "  package_size: %d\n", app_config.package_size);

    fclose(file);
    return EXIT_SUCCESS;
}

enum ConfigError parse_app_config(void)
{
    memset(&app_config, 0, sizeof(struct AppConfig));

    app_config.port[0] = 0;
    app_config.baudrate = 115200;
    app_config.package_size = 1024;

    struct IniConfig ini;
    memset(&ini, 0, sizeof(struct IniConfig));

    FILE *file;
    open_app_config(&file, "r");
    if (!open_config(&ini, &file))
    {
        // fprintf(stderr, "Can't find config divinus.yaml in: divinus.yaml, /etc/divinus.yaml\n");
        return -1;
    }

    enum ConfigError err;
    find_sections(&ini);
    err = parse_param_value(&ini, "serial", "port", app_config.port);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(&ini, "serial", "baudrate", 9600, 115200, &app_config.baudrate);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(&ini, "serial", "package_size", 512, 2048, &app_config.package_size);
    if (err != CONFIG_OK)
        goto RET_ERR;
    free(ini.str);
    return CONFIG_OK;
RET_ERR:
    free(ini.str);
    return err;
}
