#include <stdbool.h>
#include <time.h>

#define URL_BUFFER_SIZE 256
#define UART_BUFFER_SIZE 1024

void sendGetRequest(const char *text);
char *textEncode(const char *str);
bool findNearestFile(struct tm *time_info, char *path);
long int findSize(char file_name[]);
char *readBytesFromFile(const char *filename, long start_index);
bool parseDatetimeFromFile(char *filepath, struct tm *parsed_time);
int listFile(struct tm *tm_info);
void toggleLed();
