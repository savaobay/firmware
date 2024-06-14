#include "app_config.h"
#include <curl/curl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define CONFIG_FILE "/etc/serial.ini"
#define UART_BUFFER_SIZE 1024
#define URL_BUFFER_SIZE 256
#define IMAGE_PATH                                                                                                     \
    "/mnt/mmcblk0p1/2024-05-23/image/00/00-02.jpg" // Adjust the image path
                                                   // accordingly
#define SD_CARD_PATH "/mnt/mmcblk0p1"
enum Status
{
    ERROR,
    ACK,
    NACK,
    SUCCESS,
    DONE,
};
enum Mark
{
    START = 0x55,
    END = 0x23,
};

enum PackageSize
{
    SIZE_512 = 0x00,
    SIZE_1024 = 0x01,
    SIZE_2048 = 0x02,
};

enum CommandSpecifier
{
    LIST_FILE = 0x4C,
    NEXT_FILE = 0x4D,
    GET_SPEC_PACKAGE = 0x45,
    SEND_SPEC_DATA_PACKAGE = 0x46,
    BAUD_RATE = 0x49,
    OSD = 0x4F,
    RTC = 0x54,
    NONE = 0x63
};

enum BaudRate
{
    BAUD_9600 = 0x30,
    BAUD_19200 = 0x31,
    BAUD_38400 = 0x32,
    BAUD_57600 = 0x33,
    BAUD_115200 = 0x34,
    DEFAULT_BAUD = 0x00
};

struct OsdContent
{
    char position;
    char text_length;
    char *text;
};

struct Command
{
    char header;
    char command;
    char camera_id;
    char position;
    char text_length;
    char *text;
    char end;
};

struct Data
{
    char id[2];
    char size[2];
    char *data;
    char checksum[2];
};

struct CommandFrame
{
    char header;
    char command_specifier;
    char camera_id;
    char *command_content;
    char end;
};

struct AckFrame
{
    char header;
    char command_specifier;
    char camera_id;
    char optional; // optional data;
    char end;
};

struct DataFrame
{
    char header;
    char command;
    char camera_id;
    struct Data data;
    char end;
};

struct NextContent
{
    char date[3];
    char time[2];
    char size;
};

// Function to URL-encode a string
static char *text_encode(const char *str)
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
// Function to parse datetime from filename
int parse_datetime_from_filename(const char *filepath, struct tm *parsed_time)
{
    // Assuming filename format is /mnt/mmcblk0p1/2024-05-25/video/15/15/%M-%S.jpg
    // Example filename: /mnt/mmcblk0p1/2024-05-25/video/15/15/30-14.jpg
    int year, month, minute, second;
    if (sscanf(filepath, "%2d-%2d.jpg", &minute, &second) == 2)
    {
        parsed_time->tm_min = minute;
        parsed_time->tm_sec = second;
        return 0;
    }
    return -1;
}

// Function to traverse directories and find the nearest file
int find_nearest_jpg_file(time_t target_time, const char *path)
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
                }
            }
        }
        closedir(dir);
    }
    else
    {
        perror("Failed to open directory");
    }

    if (min_diff != -1)
    {
        printf("Nearest file: %s\n", nearest_file_path);
        path = nearest_file_path;
        return 0;
    }
    else
    {
        printf("No matching files found\n");
        return -1;
    }
}

void send_get_request(const char *text)
{
    CURL *curl;
    CURLcode res;

    printf("Set %s osd\n", text);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        char url[URL_BUFFER_SIZE];
        char *encoded_text = text_encode(text);
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

// Function to configure the serial port
void configure_serial_port(int fd, int baud_rate)
{
    struct termios options;

    // Get current settings
    tcgetattr(fd, &options);

    // Set baud rates
    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);

    // 8N1 mode
    options.c_cflag &= ~PARENB; // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8; // 8 data bits

    // Apply the settings
    tcsetattr(fd, TCSANOW, &options);
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

// Function to send an image file through UART
void send_image_via_uart(int uart_fd, const char *image_path)
{
    FILE *file = fopen(image_path, "rb");
    if (!file)
    {
        perror("Failed to open image file");
        return;
    }

    char buffer[UART_BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        ssize_t bytes_written = write(uart_fd, buffer, bytes_read);
        // usleep(1000);
        if (bytes_written < 0)
        {
            perror("Failed to write to UART");
            break;
        }
    }

    fclose(file);
}

// Function to flush the UART buffers
void flush_uart(int fd)
{
    // Flush both input and output buffers
    if (tcflush(fd, TCIOFLUSH) == -1)
    {
        perror("Unable to flush UART");
    }
}

int write_ack_frame(int fd, struct AckFrame *ack_frame)
{
    char frame[5] = {START, ack_frame->command_specifier, ack_frame->camera_id, ack_frame->optional, END};
    return write(fd, frame, 5);
}
int write_nack_frame(int fd, struct AckFrame *ack_frame)
{
    char frame[5] = {START, ack_frame->command_specifier, ack_frame->camera_id, ack_frame->optional, END};
    return write(fd, frame, 5);
}

int write_data_frame(int fd, struct DataFrame *data_frame)
{
    char frame[5] = {START, data_frame->command, data_frame->camera_id, data_frame->data, END};
    return write(fd, frame, 5);
}

// Function to parse data from UART buffer into Command structure

int parse_command(const char *buffer, size_t buffer_length, struct AckFrame *ack_frame)
{
    if (buffer_length < 5)
    {
        return NACK;
    }
    struct CommandFrame cmd;

    cmd.header = buffer[0];
    cmd.command_specifier = buffer[1];
    cmd.camera_id = buffer[2];
    cmd.command_content = &buffer[3];
    cmd.end = buffer[buffer_length - 1];

    printf("Header: %d\n", cmd.header);
    printf("Command specifier: %d\n", cmd.command_specifier);
    printf("Camera ID: %d\n", cmd.camera_id);
    printf("End: %d\n", cmd.end);

    switch (cmd.command_specifier)
    {
    case LIST_FILE:
        printf("List file in folder command\n");
        break;
    case NEXT_FILE:
        printf("Get next file command\n");
        if (buffer_length < 6)
        {
            return NACK;
        }
        struct tm tm_info;
        memset(&tm_info, 0, sizeof(struct tm));
        // Assuming year 2000+ for simplicity, adjust if needed
        tm_info.tm_year = 100 + cmd.command_content[0];
        tm_info.tm_mon = cmd.command_content[1] - 1; // Month is 0-based in struct tm
        tm_info.tm_mday = cmd.command_content[2];
        tm_info.tm_hour = cmd.command_content[3];
        tm_info.tm_min = cmd.command_content[4];

        time_t time = mktime(&tm_info);
        printf("Requested time: %s", asctime(&tm_info));
        char path[PATH_MAX];
        int res = find_nearest_jpg_file(time, path);
        if (res == 0)
        {
            ack_frame->header = START;
            ack_frame->command_specifier = NEXT_FILE;
            ack_frame->camera_id = cmd.camera_id;
            int size_file = findSize(path);
            int remaining = size_file / cmd.command_content[5];
            if (remaining == 0)
            {
                ack_frame->optional = size_file / cmd.command_content[5];
            }
            else
            {
                ack_frame->optional = size_file / cmd.command_content[5] + 1;
            }

            ack_frame->end = END;
        }
        else
        {
            return NACK;
        }

        break;
    case GET_SPEC_PACKAGE:
        printf("Get Specified package command\n");
        int hour = cmd.command_content[0];
        int minute = cmd.command_content[1];
        int index = cmd.command_content[2];
        // Next to send spec data package command
        // TODO: Send the specified data package
        cmd.command_specifier = SEND_SPEC_DATA_PACKAGE;

        break;
    case SEND_SPEC_DATA_PACKAGE:
        printf("Send specified data package command\n");
        struct DataFrame data;
        data.header = cmd.header;
        data.command = cmd.command_specifier;
        data.camera_id = cmd.camera_id;
        data.data.id[0] = cmd.command_content[0];
        data.data.id[1] = cmd.command_content[1];

        break;
    case BAUD_RATE:
        printf("Baud rate command\n");
        int baud_rate = cmd.command_content[0];
        break;
    case OSD:
        printf("OSD command\n");
        struct OsdContent osd;
        osd.position = cmd.command_content[0];
        osd.text_length = cmd.command_content[1];
        osd.text = malloc(osd.text_length + 1);
        memcpy(osd.text, &cmd.command_content[2], osd.text_length);
        osd.text[osd.text_length] = '\0';
        printf("OSD position: %c\n", osd.position);
        printf("OSD text length: %d\n", osd.text_length);
        printf("OSD text: %s\n", osd.text);
        send_get_request(osd.text);
        free(osd.text);
        break;
    case RTC:
        printf("RTC command\n");

        break;
    default:
        printf("Invalid command\n");
        return NACK;
    }

    return ACK;
}

// Function to free the allocated text in Command structure
void free_command(struct CommandFrame *cmd)
{
    if (cmd->command_content)
    {
        free(cmd->command_content);
        cmd->command_content = NULL;
    }
}

int main()
{
    // read config from file serial.ini
    if (parse_app_config("./divinus.ini") != CONFIG_OK)
    {
        fprintf(stderr, "Can't load app config './divinus.ini'\n");
        return EXIT_FAILURE;
    }

    struct AckFrame ack_frame;
    int uart_out_fd;
    char buffer[UART_BUFFER_SIZE];
    int num_bytes;
    char url[URL_BUFFER_SIZE];

    // Open the output serial port
    uart_out_fd = open(app_config.port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_out_fd == -1)
    {
        perror("Unable to open UART_OUT");
        return ERROR;
    }
    int baud_rate = app_config.baudrate;
    switch (baud_rate)
    {
    case 9600:
        baud_rate = B9600;
        break;
    case 19200:
        baud_rate = B19200;
        break;
    case 38400:
        baud_rate = B38400;
        break;
    case 57600:
        baud_rate = B57600;
        break;
    case 115200:
        baud_rate = B115200;
        break;
    default:
        baud_rate = B115200;
        break;
    }

    // Set up both serial ports
    configure_serial_port(uart_out_fd, baud_rate);

    // Ensure the file descriptors are blocking
    fcntl(uart_out_fd, F_SETFL, 0);

    // Flush the UART buffers before starting communication
    flush_uart(uart_out_fd);

    while (1)
    {
        // Read data from UART
        num_bytes = read(uart_out_fd, buffer, sizeof(buffer));
        if (num_bytes < 0)
        {
            perror("Read error");
            close(uart_out_fd);
            return ERROR;
        }

        if (num_bytes > 0)
        {
            buffer[num_bytes] = '\0'; // Null-terminate the string
            printf("Received: %s, length: %d\n", buffer,
                   num_bytes); // Print received data to console
            int res = parse_command(buffer, num_bytes, &ack_frame);
            // send the ack nack frame
            res == ACK ? write_ack_frame(uart_out_fd, &ack_frame) : write_nack_frame(uart_out_fd, &ack_frame);
        }
        flush_uart(uart_out_fd);
    }

    flush_uart(uart_out_fd);
    // Close the serial ports
    close(uart_out_fd);

    return SUCCESS;
}
