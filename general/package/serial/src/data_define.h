
enum Status
{
    ERROR,
    ACK,
    NACK,
    SUCCESS,
    DONE,
};

enum
{
    ACK_0 = 0x00,
    ACK_4 = 0x04,
    ACK_5 = 0x05,
    ACK_7 = 0x07,
};

enum Mark
{
    START = 0x55,
    END = 0x23,
};

enum PackageSize
{
    SIZE_256 = 0x00,
    SIZE_512 = 0x01,
    SIZE_1024 = 0x02,
    SIZE_2048 = 0x03,
};

enum CommandSpecifier
{
    LIST_FILE = 0x4C,
    NEXT_FILE = 0x4D,
    GET_SPEC_PACKAGE = 0x45,
    SEND_SPEC_DATA_PACKAGE = 0x46,
    BAUD_RATE = 0x49,
    MOSD = 0x4F,
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
    DEFAULT_BAUD = 0x34
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
    char id[3];
    char size;
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
    char hour;
    char minute;
    char optional;
    char end;
    int len;
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
