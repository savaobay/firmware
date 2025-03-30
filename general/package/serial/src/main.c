#include "app_config.h"
#include "common.h"
#include "data_define.h"
#include "region.h"
#include "serial.h"
#include "storage.h"
#include "text.h"
#include "utils.h"
#include "watchdog.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t keep_running = 1;
char graceful = 0;
void *io_map;

void handle_error(int signo)
{
    write(STDERR_FILENO, "Error occurred! Quitting...\n", 28);
    keep_running = 0;
    exit(EXIT_FAILURE);
}

void handle_exit(int signo)
{
    write(STDERR_FILENO, "Graceful shutdown...\n", 21);
    keep_running = 0;
    graceful = 1;
}

int main(int argc, char *argv[])
{
    upgrade_application_from_sdcard();

    {
        char signal_error[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
        char signal_exit[] = {SIGINT, SIGQUIT, SIGTERM};
        char signal_null[] = {EPIPE, SIGPIPE};

        for (char *s = signal_error; s < (&signal_error)[1]; s++)
            signal(*s, handle_error);
        for (char *s = signal_exit; s < (&signal_exit)[1]; s++)
            signal(*s, handle_exit);
        for (char *s = signal_null; s < (&signal_null)[1]; s++)
            signal(*s, NULL);
    }

    if (parse_app_config() != CONFIG_OK)
    {
        fprintf(stderr, "Can't load app config 'serial.yaml'\n");
        return EXIT_FAILURE;
    }

    printf("app config port: %s baudrate: %d package_size: %d threshold: %d sdcard_interval: %d watchdog: %d\n",
           app_config.port, app_config.baudrate, app_config.package_size, app_config.threshold,
           app_config.sdcard_interval, app_config.watchdog);

    int fd_mem = open("/dev/mem", O_RDWR);
    io_map = mmap(NULL, IO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mem, IO_BASE);

    static MI_RGN_PaletteTable_t g_stPaletteTable = {{{0, 0, 0, 0}}};
    int s32Ret = MI_RGN_Init(&g_stPaletteTable);
    if (s32Ret)
        fprintf(stderr, "[%s:%d]RGN_Init failed with %#x!\n", __func__, __LINE__, s32Ret);

    toggleLed();
    start_storage_handle();
    start_region_handler();
    start_serial_handler();

    while (keep_running)
    {
        watchdog_reset();
        sleep(1);
    }

    stop_serial_handler();
    stop_region_handler();
    stop_storage_handle();

    s32Ret = MI_RGN_DeInit();
    if (s32Ret)
        printf("[%s:%d]RGN_DeInit failed with %#x!\n", __func__, __LINE__, s32Ret);

    munmap(io_map, IO_SIZE);
    close(fd_mem);

    if (app_config.watchdog)
        watchdog_stop();

    if (!graceful)
        restore_app_config();

    return graceful ? EXIT_SUCCESS : EXIT_FAILURE;
}
