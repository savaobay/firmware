#ifndef REGION_H_
#define REGION_H_

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#include "bitmap.h"
#include "common.h"
#define DATA_SIZE (256)
#define DEF_COLOR 0xFFFF
#define DEF_OPAL 255
// OpenIPC font must be replaced for Unicode support
#define DEF_FONT "UbuntuMono-Regular"
#define DEF_POSX 16
#define DEF_POSY 16
#define DEF_SIZE 32.0f
#define DEF_TIMEFMT "%Y/%m/%d %H:%M:%S"
#define MAX_CONN 16
#define MAX_OSD 8
#define PORT "9000"
#define QUEUE_SIZE 1000000
#define SUPP_UTF32

    int create_region(int *handle, int x, int y, int width, int height);
    int prepare_bitmap(const char *filename, BITMAP *bitmap, int bFil, unsigned int u16FilColor, int enPixelFmt);
    int set_bitmap(int handle, BITMAP *bitmap);
    void unload_region(int *handle);
    int start_region_handler();
    void stop_region_handler();
    extern OSD osds[MAX_OSD];
    extern char timefmt[32];
    extern volatile sig_atomic_t keep_running;
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
