#include "app_config.h"
#include "data_define.h"
#include "utils.h"
#include <fcntl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#define IMAGE_PATH "/mnt/mmcblk0p1/2024-05-23/image/00/00-02.jpg"
#define SD_CARD_PATH "/mnt/mmcblk0p1"

static int uart_out_fd;
static int write_data_frame(int fd, struct DataFrame *data_frame);

// Function to configure the serial port
void configure_serial_port(int fd, int baud_rate)
{
    struct termios options;

    // Get current settings
    tcgetattr(fd, &options);

    // Set baud rates
    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);
    /*
        // 8N1 mode
        options.c_cflag &= ~PARENB; // No parity
        options.c_cflag &= ~CSTOPB; // 1 stop bit
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8; // 8 data bits
        options.c_cflag &= ~CRTSCTS;
        // Enable receiver and set local mode
        options.c_cflag |= (CLOCAL | CREAD);

        // Disable echo and canonical mode (raw mode)
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); */

    options.c_cflag &= ~CSIZE;  // Mask the character size bits
    options.c_cflag |= CS8;     // 8 bit data
    options.c_cflag &= ~PARENB; // set parity to no
    options.c_cflag &= ~PARODD; // set parity to no
    options.c_cflag &= ~CSTOPB; // set one stop bit

    options.c_cflag |= (CLOCAL | CREAD);

    options.c_oflag &= ~OPOST;

    options.c_lflag &= 0;
    options.c_iflag &= 0; // disable software flow control
    options.c_oflag &= 0;
    options.c_cc[VMIN] = 1;  // Read at least 1 character
    options.c_cc[VTIME] = 1; // Timeout in deciseconds
    cfmakeraw(&options);
    // Apply the settings
    tcsetattr(fd, TCSANOW, &options);
}

// Function to read a line from UART
ssize_t readline(int fd, char *buffer, size_t size)
{
    ssize_t total_bytes_read = 0;
    ssize_t bytes_read;
    char ch;

    while (total_bytes_read < size - 1)
    {
        bytes_read = read(fd, &ch, 1);
        if (bytes_read < 0)
        {
            perror("Error reading from UART");
            return -1;
        }
        else if (bytes_read == 0)
        {
            // End of file or no more data
            break;
        }

        buffer[total_bytes_read++] = ch;

        if (ch == '\n' && buffer[total_bytes_read - 2] == '\r')
        {
            // End of line
            break;
        }
    }

    buffer[total_bytes_read] = '\0'; // Null-terminate the string
    return total_bytes_read;
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

// Function to parse data from UART buffer into Command structure

void parse_command(const char *buffer, size_t buffer_length, struct AckFrame *ack_frame)
{
    if (buffer_length < 6)
    {
        ack_frame->len = ACK_0;
        ack_frame->command_specifier = NONE;
        return;
    }
    struct CommandFrame cmd;

    cmd.header = buffer[0];
    cmd.command_specifier = buffer[1];
    cmd.camera_id = buffer[2];
    cmd.command_content = &buffer[3];
    cmd.end = buffer[buffer_length - 3]; // ignore the last byte

    printf("Header: %02X\n", cmd.header);
    printf("Command specifier: %02X\n", cmd.command_specifier);
    printf("Camera ID: %02X\n", cmd.camera_id);
    printf("End: %02X\n", cmd.end);

    ack_frame->header = START;
    ack_frame->camera_id = cmd.camera_id;
    ack_frame->command_specifier = cmd.command_specifier;
    ack_frame->end = END;

    switch (cmd.command_specifier)
    {
    case LIST_FILE:
        printf("List file in folder command\n");
        if (buffer_length != 11)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_5;
        struct tm tm_if;
        memset(&tm_if, 0, sizeof(struct tm));
        tm_if.tm_year = 100 + cmd.command_content[0];
        tm_if.tm_mon = cmd.command_content[1] - 1; // Month is 0-based in struct tm
        tm_if.tm_mday = cmd.command_content[2];
        tm_if.tm_hour = cmd.command_content[3];
        tm_if.tm_min = 0;
        tm_if.tm_sec = 0;
        int files = listFile(&tm_if);
        printf("Number of files: %d\n", files);
        ack_frame->optional = files;
        break;

    case NEXT_FILE:
        printf("Get next file command\n");
        if (buffer_length != 12)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_7;
        struct tm tm_info;
        memset(&tm_info, 0, sizeof(struct tm));
        // Assuming year 2000+ for simplicity, adjust if needed
        tm_info.tm_year = 100 + cmd.command_content[0];
        tm_info.tm_mon = cmd.command_content[1] - 1; // Month is 0-based in struct tm
        tm_info.tm_mday = cmd.command_content[2];
        tm_info.tm_hour = cmd.command_content[3];
        tm_info.tm_min = cmd.command_content[4];

        printf("tm: %d-%d-%d %d:%d\n", tm_info.tm_year, tm_info.tm_mon, tm_info.tm_mday, tm_info.tm_hour,
               tm_info.tm_min);
        printf("Requested time: %s\n", asctime(&tm_info));

        char path[PATH_MAX];
        memset(path, 0, PATH_MAX);
        if (findNearestFile(&tm_info, path))
        {
            parseDatetimeFromFile(path, &tm_info);
            ack_frame->hour = tm_info.tm_hour;
            ack_frame->minute = tm_info.tm_min;
            printf("Path: %s\n", path);
            int size_file = findSize(path);
            int package_size = SIZE_1024;
            switch (cmd.command_content[5])
            {
            case SIZE_512:
                package_size = 512;
                break;
            case SIZE_1024:
                package_size = 1024;
                break;
            case SIZE_2048:
                package_size = 2048;
                break;
            default:
                package_size = 1024;
            }
            app_config.package_size = package_size;
            int remaining = size_file / package_size;
            if (remaining == 0)
            {
                ack_frame->optional = size_file / package_size;
            }
            else
            {
                ack_frame->optional = size_file / package_size + 1;
            }
            printf("Number of packages: %d\n", ack_frame->optional);
        }
        // else
        // {
        //     ack_frame->command_specifier = NONE;
        // }
        break;

    case GET_SPEC_PACKAGE:
        printf("Get Specified package command\n");
        if (buffer_length != 9)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_4;
        int hour = cmd.command_content[0];
        int minute = cmd.command_content[1];
        int package_no = cmd.command_content[2];
        cmd.command_specifier = SEND_SPEC_DATA_PACKAGE;
        // send ack

    case SEND_SPEC_DATA_PACKAGE:
        printf("Send specified data package command\n");
        ack_frame->len = ACK_0;
        struct DataFrame data_frame;
        data_frame.header = cmd.header;
        data_frame.command = cmd.command_specifier;
        data_frame.camera_id = cmd.camera_id;

        struct Data data;
        data.id[0] = cmd.command_content[0];
        data.id[1] = cmd.command_content[1];
        data.id[2] = cmd.command_content[2]; // Index package
        data.data = readBytesFromFile(path, (package_no - 1) * UART_BUFFER_SIZE);
        memset(path, 0, PATH_MAX);
        if (data.data == NULL)
        {
            // ack_frame->command_specifier = NONE;
            free(data.data);
            break;
        }
        switch (app_config.package_size)
        {
        case 512:
            data.size = SIZE_512;
            break;
        case 1024:
            data.size = SIZE_1024;
            break;
        case 2048:
            data.size = SIZE_2048;
            break;
        default:
            data.size = SIZE_1024;
        }

        // checksum = header + cmd + camera_id + index + data_size + data and get the last 2 bytes
        long checksum = data_frame.header + data_frame.command + data_frame.camera_id + package_no + data.size;
        for (int i = 0; i < UART_BUFFER_SIZE; i++)
        {
            checksum += data.data[i];
        }
        data.checksum[0] = (checksum >> 8) & 0xFF;
        data.checksum[1] = checksum & 0xFF;
        data_frame.data = data;
        data_frame.end = END;
        // send package data
        write_data_frame(uart_out_fd, &data_frame);
        free(data.data);
        break;

    case BAUD_RATE:
        printf("Baud rate command\n");
        if (buffer_length != 7)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_4;
        int baud_rate = cmd.command_content[0];
        break;

    case OSD:
        printf("OSD command\n");
        if (buffer_length < 8)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_4;
        struct OsdContent osd;
        osd.position = cmd.command_content[0];
        osd.text_length = cmd.command_content[1];
        osd.text = malloc(osd.text_length + 1);
        memcpy(osd.text, &cmd.command_content[2], osd.text_length);
        osd.text[osd.text_length] = '\0';
        printf("OSD position: %c\n", osd.position);
        printf("OSD text length: %d\n", osd.text_length);
        printf("OSD text: %s\n", osd.text);
        sendGetRequest(osd.text);
        free(osd.text);
        break;

    case RTC:
        printf("RTC command\n");
        if (buffer_length != 10)
        {
            ack_frame->len = ACK_0;
            ack_frame->command_specifier = NONE;
            return;
        }
        ack_frame->len = ACK_4;
        int t1 = cmd.command_content[0];
        int t2 = cmd.command_content[1];
        int t3 = cmd.command_content[2];
        int t4 = cmd.command_content[3];
        int unix_time = t1 << 24 | t2 << 16 | t3 << 8 | t4;
        printf("Unix time: %d\n", unix_time);

        struct timeval tv;
        tv.tv_sec = unix_time;
        tv.tv_usec = 0;
        // Set the system time
        if (settimeofday(&tv, NULL) == 0)
        {
            printf("System time updated successfully.\n");
        }
        else
        {
            perror("Failed to set system time");
        }
        break;

    default:
        printf("Invalid command\n");
        ack_frame->len = ACK_4;
        ack_frame->command_specifier = NONE;
        break;
    }
}

int write_ack_frame(int fd, struct AckFrame *ack_frame)
{
    if (ack_frame->len == ACK_0)
    {
        return 0;
    }
    else if (ack_frame->len == ACK_4)
    {
        char frame[4] = {START, ack_frame->command_specifier, ack_frame->camera_id, END};
        printf("\nframe to send: 0x%02X 0x%02X 0x%02X 0x%02X\n", frame[0], frame[1], frame[2], frame[3]);
        return write(fd, frame, sizeof(frame));
    }
    else if (ack_frame->len == ACK_5)
    {
        char frame[5] = {START, ack_frame->command_specifier, ack_frame->camera_id, ack_frame->optional, END};
        printf("frame to send: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", frame[0], frame[1], frame[2], frame[3], frame[4]);
        return write(fd, frame, sizeof(frame));
    }
    else
    {
        char frame[7] = {START,
                         ack_frame->command_specifier,
                         ack_frame->camera_id,
                         ack_frame->hour,
                         ack_frame->minute,
                         ack_frame->optional,
                         END};
        printf("\nframe to send: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", frame[0], frame[1], frame[2],
               frame[3], frame[4], frame[5], frame[6]);
        return write(fd, frame, sizeof(frame));
    }
}

// Function to serialize a DataFrame into a byte array
void serialize_data_frame(struct DataFrame *data_frame, char *buffer, int *buffer_size)
{
    int index = 0;

    // Copy header, command, camera_id directly
    buffer[index++] = data_frame->header;
    buffer[index++] = data_frame->command;
    buffer[index++] = data_frame->camera_id;

    // Copy struct Data fields
    memcpy(&buffer[index], data_frame->data.id, sizeof(data_frame->data.id));
    index += sizeof(data_frame->data.id);

    memcpy(&buffer[index], (const char *)&data_frame->data.size, sizeof(data_frame->data.size));
    index += sizeof(data_frame->data.size);

    // Copy data

    // memcpy(&buffer[index], data_frame->data.data, data_frame->data.size);
    // index += data_frame->data.size;

    memcpy(&buffer[index], data_frame->data.data, UART_BUFFER_SIZE);
    index += UART_BUFFER_SIZE;

    memcpy(&buffer[index], data_frame->data.checksum, sizeof(data_frame->data.checksum));
    index += sizeof(data_frame->data.checksum);

    // Copy end marker
    buffer[index++] = data_frame->end;

    *buffer_size = index; // Set the buffer size
}

static int write_data_frame(int fd, struct DataFrame *data_frame)
{
    flush_uart(fd);
    char buffer[1033]; // Assuming a reasonable buffer size
    int buffer_size = 0;
    serialize_data_frame(data_frame, buffer, &buffer_size);
    printf("buffer size: %d\n", buffer_size);
    for (int i = 0; i < buffer_size; i++)
    {
        // printf("0x%02X ", buffer[i]);
        write(fd, &buffer[i], 1);
        usleep(100);
    }
    // int result = write(fd, &buffer, sizeof(buffer));
    return 0;
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
    if (parse_app_config() != CONFIG_OK)
    {
        fprintf(stderr, "Can't load app config 'serial.yaml'\n");
        return EXIT_FAILURE;
    }
    printf("app config port: %s baudrate: %d package_size: %d\n", app_config.port, app_config.baudrate,
           app_config.package_size);

    // Open the output serial port
    uart_out_fd = open(app_config.port, O_RDWR | O_NOCTTY | O_NDELAY | O_SYNC);

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

    // Flush the UART buffers before starting communication
    flush_uart(uart_out_fd);

    // Set up both serial ports
    configure_serial_port(uart_out_fd, baud_rate);

    // Ensure the file descriptors are blocking
    fcntl(uart_out_fd, F_SETFL, 0);

    toggleLed();

    while (true)
    {
        struct AckFrame ack_frame;
        char buffer[256];

        memset(buffer, 0, sizeof(buffer));
        // Read data from UART
        ssize_t num_bytes = readline(uart_out_fd, buffer, sizeof(buffer));
        if (num_bytes < 0)
        {
            perror("Read error");
            close(uart_out_fd);
            return ERROR;
        }
        else if (num_bytes == 0)
        {
            continue;
        }
        // check end buffer is newline character
        else
        {
            toggleLed();
            memset(&ack_frame, 0, sizeof(struct AckFrame));
            buffer[num_bytes] = '\0'; // Null-terminate the string
            printf("Received length: %ld\n", num_bytes);
            for (int i = 0; i < num_bytes; i++)
            {
                printf("0x%02X ", buffer[i]);
            }
            parse_command(buffer, num_bytes, &ack_frame);
            write_ack_frame(uart_out_fd, &ack_frame);
        }
        flush_uart(uart_out_fd);
    }

    flush_uart(uart_out_fd);

    close(uart_out_fd);

    return SUCCESS;
}
