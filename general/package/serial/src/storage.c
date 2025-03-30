#include "storage.h"
#include "app_config.h"
#include "utils.h"
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 4096
pthread_t storagePid = 0;

typedef struct
{
    char path[MAX_PATH_LENGTH];
    time_t mtime;
    off_t size;
} FileInfo;

static double get_storage_usage_percent(void)
{
    struct statvfs fs_stats;

    if (statvfs(SD_CARD_PATH, &fs_stats) != 0)
    {
        return -1.0;
    }

    unsigned long total_blocks = fs_stats.f_blocks;
    unsigned long used_blocks = total_blocks - fs_stats.f_bfree;
    return ((double)used_blocks / total_blocks) * 100;
}

static void process_files(const char *base_path, const char *date_dir, const char *type, FileInfo **files, int *count,
                          int *capacity)
{
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s/%s", base_path, date_dir, type);

    DIR *hour_dir;
    struct dirent *hour_entry;

    hour_dir = opendir(full_path);
    if (!hour_dir)
    {
        return;
    }

    while ((hour_entry = readdir(hour_dir)) != NULL)
    {
        if (hour_entry->d_type != DT_DIR || strcmp(hour_entry->d_name, ".") == 0 ||
            strcmp(hour_entry->d_name, "..") == 0)
        {
            continue;
        }

        char hour_path[MAX_PATH_LENGTH];
        snprintf(hour_path, sizeof(hour_path), "%s/%s", full_path, hour_entry->d_name);

        DIR *content_dir;
        struct dirent *content_entry;

        content_dir = opendir(hour_path);
        if (!content_dir)
        {
            continue;
        }

        while ((content_entry = readdir(content_dir)) != NULL)
        {
            if (content_entry->d_type != DT_REG)
            {
                continue;
            }

            char file_path[MAX_PATH_LENGTH];
            snprintf(file_path, sizeof(file_path), "%s/%s", hour_path, content_entry->d_name);

            struct stat st;
            if (stat(file_path, &st) == 0)
            {
                if (*count >= *capacity)
                {
                    *capacity *= 2;
                    FileInfo *temp = realloc(*files, sizeof(FileInfo) * *capacity);
                    if (!temp)
                    {
                        closedir(content_dir);
                        closedir(hour_dir);
                        return;
                    }
                    *files = temp;
                }

                strncpy((*files)[*count].path, file_path, MAX_PATH_LENGTH - 1);
                (*files)[*count].mtime = st.st_mtime;
                (*files)[*count].size = st.st_size;
                (*count)++;
            }
        }
        closedir(content_dir);
    }
    closedir(hour_dir);
}

static FileInfo *find_oldest_files(const char *base_path, int *count)
{
    FileInfo *files = NULL;
    int capacity = 100;
    *count = 0;

    files = malloc(capacity * sizeof(FileInfo));
    if (!files)
    {
        return NULL;
    }

    // Process all date directories
    struct dirent *entry;
    DIR *d = opendir(base_path);
    if (!d)
    {
        free(files);
        return NULL;
    }

    while ((entry = readdir(d)) != NULL)
    {
        if (entry->d_type != DT_DIR)
        {
            continue;
        }

        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Check if it's a date directory (YYYY-MM-DD format)
        if (strlen(entry->d_name) != 10 || entry->d_name[4] != '-' || entry->d_name[7] != '-')
        {
            continue;
        }

        // Process both video and image directories
        process_files(base_path, entry->d_name, "video", &files, count, &capacity);
        process_files(base_path, entry->d_name, "image", &files, count, &capacity);

        if (*count >= capacity)
        {
            FileInfo *temp = realloc(files, capacity * 2 * sizeof(FileInfo));
            if (!temp)
            {
                free(files);
                closedir(d);
                return NULL;
            }
            files = temp;
            capacity *= 2;
        }
    }

    closedir(d);

    if (*count == 0)
    {
        free(files);
        return NULL;
    }

    return files;
}

static int compare_files(const void *a, const void *b)
{
    const FileInfo *fa = (const FileInfo *)a;
    const FileInfo *fb = (const FileInfo *)b;
    return (fa->mtime - fb->mtime);
}

static void remove_empty_directory(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        return;
    }

    struct dirent *entry;
    bool is_empty = true;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            is_empty = false;
            break;
        }
    }
    closedir(dir);

    if (is_empty)
    {
        rmdir(path);
    }
}

static void cleanup_empty_directories(const char *base_path)
{
    DIR *dir = opendir(base_path);
    if (!dir)
    {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char date_path[MAX_PATH_LENGTH];
        snprintf(date_path, sizeof(date_path), "%s/%s", base_path, entry->d_name);

        // Check video and image subdirectories
        char type_paths[2][MAX_PATH_LENGTH];
        snprintf(type_paths[0], sizeof(type_paths[0]), "%s/video", date_path);
        snprintf(type_paths[1], sizeof(type_paths[1]), "%s/image", date_path);

        for (int i = 0; i < 2; i++)
        {
            DIR *type_dir = opendir(type_paths[i]);
            if (!type_dir)
            {
                continue;
            }
            closedir(type_dir);

            struct dirent *hour_entry;
            DIR *hour_dir = opendir(type_paths[i]);
            if (hour_dir)
            {
                while ((hour_entry = readdir(hour_dir)) != NULL)
                {
                    if (hour_entry->d_type == DT_DIR && strcmp(hour_entry->d_name, ".") != 0 &&
                        strcmp(hour_entry->d_name, "..") != 0)
                    {

                        char hour_path[MAX_PATH_LENGTH];
                        snprintf(hour_path, sizeof(hour_path), "%s/%s", type_paths[i], hour_entry->d_name);
                        remove_empty_directory(hour_path);
                    }
                }
                closedir(hour_dir);
                remove_empty_directory(type_paths[i]);
            }
        }
        remove_empty_directory(date_path);
    }
    closedir(dir);
}

void *storage_thread(void *arg)
{
    printf("Storage service started (checking every %d seconds)\n", app_config.sdcard_interval);

    while (keep_running)
    {
        // Check if SD card is mounted
        if (mount_sdcard() == 0)
        {
            double usage_percent = get_storage_usage_percent();

            if (usage_percent > app_config.threshold)
            {
                printf("Storage usage (%.1f%%) exceeds threshold (%d%%)\n", usage_percent, app_config.threshold);

                int file_count = 0;
                FileInfo *files = find_oldest_files(SD_CARD_PATH, &file_count);

                if (files && file_count > 0)
                {
                    // Sort files by modification time
                    qsort(files, file_count, sizeof(FileInfo), compare_files);

                    // Delete oldest file
                    if (unlink(files[0].path) == 0)
                    {
                        printf("Deleted old file: %s\n", files[0].path);

                        // Clean up empty directories after file deletion
                        cleanup_empty_directories(SD_CARD_PATH);

                        // Check new usage after deletion
                        double new_usage = get_storage_usage_percent();
                        printf("New storage usage: %.1f%%\n", new_usage);
                    }
                    else
                    {
                        fprintf(stderr, "Failed to delete file: %s\n", files[0].path);
                    }
                }
                else
                {
                    fprintf(stderr, "No files found to delete\n");
                }
                free(files);
            }
        }
        sleep(app_config.sdcard_interval);
    }

    printf("Storage service stopped\n");
    return NULL;
}

void *start_storage_handle()
{
    printf("start storage thread\n");
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 320 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
    {
        printf("[storage] Can't set stack size %zu\n", new_stacksize);
    }
    pthread_create(&storagePid, &thread_attr, (void *(*)(void *))storage_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
    {
        printf("[storage] Error:  Can't set stack size %zu\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}

void stop_storage_handle(void)
{
    pthread_join(storagePid, NULL);
}
