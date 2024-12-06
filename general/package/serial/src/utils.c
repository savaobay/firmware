// Function to URL-encode a string
#include "utils.h"
#include "region.h"
#include <curl/curl.h>
#include <dirent.h>
#include <limits.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

static inline void sha256_file(const char *filename, unsigned char *hash);
static inline int copy_upgrade_file(const char *src, const char *dst);
static inline int validate_upgrade_file(const char *file_path);

int mount_sdcard(void)
{
    // check if sd card is mounted
    struct stat st;
    if (stat(SD_CARD_PATH, &st) == 0)
    {
        fprintf(stderr, "SD card is already mounted\n");
        return 0;
    }
    if (mount("/dev/mmcblk0p1", SD_CARD_PATH, "vfat", 0, NULL) != 0)
    {
        perror("Failed to mount SD card");
        return -1;
    }
    return 0;
}
// Function to calculate the SHA-256 hash of a file
static inline void sha256_file(const char *filename, unsigned char *hash)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("File opening failed");
        return;
    }

    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts_ret(&sha256_ctx, 0); // 0 means not in "SHA-224" mode (using SHA-256 mode)

    unsigned char buffer[BUF_SIZE];
    size_t bytesRead = 0;

    while ((bytesRead = fread(buffer, 1, BUF_SIZE, file)) > 0)
    {
        mbedtls_sha256_update_ret(&sha256_ctx, buffer, bytesRead);
    }

    mbedtls_sha256_finish_ret(&sha256_ctx, hash);

    fclose(file);
    mbedtls_sha256_free(&sha256_ctx);
}

static inline int copy_upgrade_file(const char *src, const char *dst)
{
    int result = system("mv /mnt/mmcblk0p1/serial /usr/bin/serial");
    if (result != 0)
    {
        perror("Failed to copy upgrade file");
        return -1;
    }
}

static inline int validate_upgrade_file(const char *file_path)
{
    struct stat st;
    if (stat(file_path, &st) != 0 || st.st_size == 0)
    {
        fprintf(stderr, "Invalid upgrade file\n");
        return -1;
    }
    unsigned char old[32]; // 32 bytes for SHA-256
    unsigned char new[32]; // 32 bytes for SHA-256

    sha256_file(APP_PATH, old);
    sha256_file(UPGRADE_FILE_PATH, new);
    if (memcmp(old, new, 32) == 0)
    {
        fprintf(stderr, "Upgrade file is the same as the current application\n");
        return -1;
    }
    return 0;
}

char *textEncode(const char *str)
{
    char *encoded = malloc(URL_BUFFER_SIZE);
    char *penc = encoded;
    const char *pstr = str;
    while (*pstr)
    {
        if (('a' <= *pstr && *pstr <= 'z') || ('A' <= *pstr && *pstr <= 'Z') || ('0' <= *pstr && *pstr <= '9'))
        {
            *penc++ = *pstr;
        }
        else
        {
            penc += sprintf(penc, "%%%02X", (unsigned char)*pstr);
        }
        pstr++;
    }
    *penc = '\0';
    return encoded;
}

void sendGetRequest(const char *text)
{
    CURL *curl;
    CURLcode res;

    printf("Set %s osd\n", text);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        char url[URL_BUFFER_SIZE];
        char *encoded_text = textEncode(text);
        snprintf(url, sizeof(url), "http://127.0.0.1:9000/api/osd/0?text=%s", encoded_text);
        // Send the GET request
        free(encoded_text);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    else
    {
        fprintf(stderr, "curl_easy_init() failed\n");
    }
    curl_global_cleanup();
}

// Function to traverse directories and find the nearest file
bool findNearestFile(struct tm *time_info, char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char filepath[PATH_MAX];
    double min_diff = -1;
    struct tm parsed_time;
    time_t file_time;
    struct stat nearest_file_stat;
    char nearest_file_path[PATH_MAX] = "";
    // get directory path from target time
    char directory[PATH_MAX];
    time_t target_time = mktime(time_info);
    strftime(directory, sizeof(directory), "/mnt/mmcblk0p1/%Y-%m-%d/image/%H", localtime(&target_time));

    if ((dir = opendir(directory)) != NULL)
    {
        while ((entry = readdir(dir)) != NULL)
        {
            if (entry->d_type == DT_REG)
            { // Only regular files
                snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);
                if (stat(filepath, &file_stat) == 0)
                {
                    if (difftime(file_stat.st_mtime, target_time) >= 0)
                    {
                        double diff = difftime(file_stat.st_mtime, target_time);
                        if (min_diff == -1 || diff < min_diff)
                        {
                            min_diff = diff;
                            strncpy(nearest_file_path, filepath, sizeof(nearest_file_path) - 1);
                            nearest_file_path[sizeof(nearest_file_path) - 1] = '\0';
                        }
                    }
                }
                else
                {
                    perror("Failed to get file stats");
                    return false;
                }
            }
        }
        closedir(dir);
    }
    else
    {
        perror("Failed to open directory");
        return false;
    }

    if (min_diff != -1)
    {
        printf("Nearest file: %s\n", nearest_file_path);
        // copy nearest_file_path to path
        strncpy(path, nearest_file_path, PATH_MAX - 1);
        return true;
    }
    else
    {
        printf("No matching files found\n");
        return false;
    }
}

long int findSize(char file_name[])
{
    // opening the file in read mode
    FILE *fp = fopen(file_name, "r");

    // checking if the file exist or not
    if (fp == NULL)
    {
        printf("File Not Found!\n");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);

    // calculating the size of the file
    long int res = ftell(fp);

    // closing the file
    fclose(fp);

    return res;
}

char *readBytesFromFile(const char *filename, int max_size, long start_index)
{
    FILE *fp;
    long file_size;
    char *buffer;
    // Open the file for reading in binary mode
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    // Determine the file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    printf("file size: %ld\n", file_size);
    fseek(fp, 0, SEEK_SET); // Reset file position to the beginning

    // Check if start_index + num_bytes exceeds file size
    if (start_index < 0 || start_index >= file_size)
    {
        fprintf(stderr, "Error: Requested bytes exceed file size\n");
        fclose(fp);
        return NULL;
    }
    // if (start_index + UART_BUFFER_SIZE > file_size)

    // Allocate buffer for reading bytes
    buffer = (char *)calloc(1, max_size * sizeof(char));
    if (buffer == NULL)
    {
        perror("Memory allocation failed");
        fclose(fp);
        return NULL;
    }

    // Move file pointer to start_index
    fseek(fp, start_index, SEEK_SET);

    // Read num_bytes bytes into buffer
    size_t bytesRead = fread(buffer, 1, max_size, fp);

    // Output the read bytes (example: print as hexadecimal)
    printf("Read %d bytes starting from index %ld:\n", max_size, start_index);
    for (long i = 0; i < max_size; i++)
    {
        printf("%02X ", (unsigned char)buffer[i]);
    }
    printf("\n");
    // copy buffer to data
    fclose(fp);
    return buffer;
}

// Function to parse datetime from filename
bool parseDatetimeFromFile(char *filepath, struct tm *parsed_time)
{
    // Example filename: /mnt/mmcblk0p1/2024-09-18/image/%02d/%02d-%02d.jpg
    int hour, minute, second;
    char *token;
    char *file_path = malloc(strlen(filepath) + 1);
    strcpy(file_path, filepath);

    token = strtok(file_path, "/");
    token = strtok(NULL, "/");
    token = strtok(NULL, "/");
    token = strtok(NULL, "/");
    token = strtok(NULL, "/");

    if (token != NULL)
    {
        hour = atoi(token);
        parsed_time->tm_hour = hour;
    }

    token = strtok(NULL, "/");
    if (token != NULL)
    {
        char *sub_token = strtok(token, "-");
        if (sub_token != NULL)
        {
            minute = atoi(sub_token);
            parsed_time->tm_min = minute;
        }

        sub_token = strtok(NULL, "-");
        if (sub_token != NULL)
        {
            second = atoi(sub_token);
            parsed_time->tm_sec = second;
        }
    }
    free(file_path);
    return true;
}

int listFile(struct tm *tm_info)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    snprintf(path, sizeof(path), "/mnt/mmcblk0p1/%04d-%02d-%02d/image/%02d", tm_info->tm_year + 1900,
             tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour);
    DIR *d;
    int count = 0;
    struct dirent *dir;
    d = opendir(path);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG)
            {
                count++;
                printf("%s\n", dir->d_name);
            }
        }
        closedir(d);
    }
    return count;
}
void toggleLed(void)
{
    int result = system("gpio toggle 0");

    if (result == -1)
    {
        // system() returned an error
        printf("Failed to execute command.\n");
    }
}

void restart_application(void)
{
    graceful = 1;
    printf("Restarting application\n");
    execl(APP_PATH, APP_PATH, (char *)NULL);
    perror("Failed to restart application");
}

void upgrade_application_from_sdcard(void)
{
    if (mount_sdcard() != 0)
    {
        return;
    }

    if (validate_upgrade_file(UPGRADE_FILE_PATH) != 0)
    {
        return;
    }

    if (copy_upgrade_file(UPGRADE_FILE_PATH, APP_PATH) != 0)
    {
        return;
    }

    restart_application();
}
