#include "snapshot.h"
#include "mi_venc.h"
#include "mi_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define VENC_CHN        1
#define VENC_WIDTH      1920
#define VENC_HEIGHT     1080
#define SNAPSHOT_TIMEOUT 3000 // 3 seconds timeout
#define DEFAULT_INTERVAL 5    // 5 seconds default interval

static MI_VENC_ChnAttr_t stVencChnAttr;
static bool is_initialized = false;
static bool service_running = false;
static pthread_t snapshot_thread;
static int snapshot_interval = DEFAULT_INTERVAL;

static void create_directory_path(const char *filepath) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", filepath);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    }
    mkdir(tmp, S_IRWXU);
}

static char* generate_snapshot_path(void) {
    time_t now;
    struct tm *tm_info;
    static char filepath[256];

    time(&now);
    tm_info = localtime(&now);

    snprintf(filepath, sizeof(filepath),
             "/mnt/mmcblk0p1/%04d-%02d-%02d/image/%02d/%02d-%02d.jpg",
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    return filepath;
}

static int init_snapshot(void) {
    MI_S32 s32Ret = 0;

    if (is_initialized) {
        return 0;
    }

    // Initialize VENC channel attributes for JPEG
    memset(&stVencChnAttr, 0, sizeof(MI_VENC_ChnAttr_t));
    stVencChnAttr.stVeAttr.eType = E_MI_VENC_MODTYPE_JPEGE;
    stVencChnAttr.stVeAttr.stAttrJpeg.u32MaxPicWidth = VENC_WIDTH;
    stVencChnAttr.stVeAttr.stAttrJpeg.u32MaxPicHeight = VENC_HEIGHT;
    stVencChnAttr.stVeAttr.stAttrJpeg.u32PicWidth = VENC_WIDTH;
    stVencChnAttr.stVeAttr.stAttrJpeg.u32PicHeight = VENC_HEIGHT;
    stVencChnAttr.stVeAttr.stAttrJpeg.u32BufSize = VENC_WIDTH * VENC_HEIGHT * 2;
    stVencChnAttr.stVeAttr.stAttrJpeg.bByFrame = TRUE;

    // Create VENC channel
    s32Ret = MI_VENC_CreateChn(VENC_CHN, &stVencChnAttr);
    if (s32Ret != MI_SUCCESS) {
        printf("Failed to create VENC channel: 0x%x\n", s32Ret);
        return -1;
    }

    // Start VENC channel
    s32Ret = MI_VENC_StartRecvPic(VENC_CHN);
    if (s32Ret != MI_SUCCESS) {
        printf("Failed to start VENC channel: 0x%x\n", s32Ret);
        MI_VENC_DestroyChn(VENC_CHN);
        return -1;
    }

    is_initialized = true;
    return 0;
}

static void cleanup_snapshot(void) {
    if (!is_initialized) {
        return;
    }

    MI_VENC_StopRecvPic(VENC_CHN);
    MI_VENC_DestroyChn(VENC_CHN);
    is_initialized = false;
}

SnapshotResult take_snapshot(void) {
    SnapshotResult result = {0};
    MI_S32 s32Ret;
    MI_VENC_Stream_t stStream;
    MI_VENC_ChnStat_t stStat;
    FILE *pFile = NULL;
    char *filepath;

    if (!is_initialized) {
        if (init_snapshot() != 0) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "Failed to initialize snapshot");
            return result;
        }
    }

    filepath = generate_snapshot_path();
    create_directory_path(filepath);

    // Reset state
    memset(&stStream, 0, sizeof(stStream));
    s32Ret = MI_VENC_GetStream(VENC_CHN, &stStream, SNAPSHOT_TIMEOUT);
    if (s32Ret != MI_SUCCESS) {
        cleanup_snapshot(); // Add cleanup
        snprintf(result.error_message, sizeof(result.error_message),
                "Failed to get stream: 0x%x", s32Ret);
        return result;
    }

    pFile = fopen(filepath, "wb");
    if (pFile == NULL) {
        MI_VENC_ReleaseStream(VENC_CHN, &stStream); // Add cleanup
        cleanup_snapshot();
        snprintf(result.error_message, sizeof(result.error_message),
                "Failed to open file: %s", strerror(errno));
        return result;
    }

    fwrite(stStream.pstPack->pu8Addr, 1, stStream.pstPack->u32Len, pFile);
    fclose(pFile);

    MI_VENC_ReleaseStream(VENC_CHN, &stStream);
    cleanup_snapshot();

    result.success = true;
    return result;
}

static void *snapshot_thread_func(void *arg) {
    printf("Snapshot service started (interval: %d seconds)\n", snapshot_interval);

    while (service_running) {
        SnapshotResult result = take_snapshot();
        if (result.success) {
            printf("Snapshot saved: %s\n", result.filepath);
        } else {
            fprintf(stderr, "Snapshot failed: %s\n", result.error_message);
        }

        sleep(snapshot_interval);
    }

    printf("Snapshot service stopped\n");
    return NULL;
}

int start_snapshot_service(void) {
    if (service_running) {
        return 0;  // Already running
    }

    if (init_snapshot() != 0) {
        return -1;
    }

    service_running = true;
    if (pthread_create(&snapshot_thread, NULL, snapshot_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create snapshot thread\n");
        service_running = false;
        cleanup_snapshot();
        return -1;
    }

    return 0;
}

void stop_snapshot_service(void) {
    if (!service_running) {
        return;
    }

    service_running = false;
    pthread_join(snapshot_thread, NULL);
    cleanup_snapshot();
}

void set_snapshot_interval(int seconds) {
    if (seconds > 0) {
        snapshot_interval = seconds;
    }
}

int get_snapshot_interval(void) {
    return snapshot_interval;
}
