#include <stdbool.h>
#include <time.h>

#define URL_BUFFER_SIZE 256
#define UART_BUFFER_SIZE 1024

#define SD_CARD_PATH "/mnt/mmcblk0p1"
#define APP_PATH "/usr/bin/serial"
#define TEMP_UPGRADE_PATH "/tmp/serial"
#define UPGRADE_FILE_PATH "/mnt/mmcblk0p1/serial"
#define BUF_SIZE 32768 // Buffer size for reading the file

extern char graceful;

void sendGetRequest(const char *text);
char *textEncode(const char *str);
bool findNearestFile(struct tm *time_info, char *path);
long int findSize(char file_name[]);
char *readBytesFromFile(const char *filename, int max_size, long start_index);
bool parseDatetimeFromFile(char *filepath, struct tm *parsed_time);
int listFile(struct tm *tm_info);
void toggleLed(void);
void restart_application(void);
void upgrade_application_from_sdcard(void);
int mount_sdcard(void);
