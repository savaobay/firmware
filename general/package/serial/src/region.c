#include "region.h"
#include "common.h"
#include "pthread.h"
#include "text.h"

const double inv16 = 1.0 / 16.0;
char timefmt[32] = DEF_TIMEFMT;
OSD osds[MAX_OSD];
pthread_t regionPid = 0;

int create_region(int *handle, int x, int y, int width, int height)
{
    int s32Ret = -1;
    MI_RGN_ChnPort_t stChn;

    MI_RGN_Attr_t stRegionCurrent;
    MI_RGN_Attr_t stRegion;

    MI_RGN_ChnPortParam_t stChnAttr;
    MI_RGN_ChnPortParam_t stChnAttrCurrent;

    memset(&stChn, 0, sizeof(MI_RGN_ChnPort_t));
    memset(&stChnAttr, 0, sizeof(MI_RGN_ChnPortParam_t));
    memset(&stChnAttrCurrent, 0, sizeof(MI_RGN_ChnPortParam_t));
    memset(&stRegionCurrent, 0, sizeof(MI_RGN_Attr_t));
    memset(&stRegion, 0, sizeof(MI_RGN_Attr_t));

    stChn.eModId = E_MI_RGN_MODID_VPE;
    stChn.s32DevId = 0;
    stChn.s32ChnId = 0;
    stChn.s32OutputPortId = 0;
    stRegion.eType = E_MI_RGN_TYPE_OSD;
    stRegion.stOsdInitParam.stSize.u32Height = height;
    stRegion.stOsdInitParam.stSize.u32Width = width;
    stRegion.stOsdInitParam.ePixelFmt = PIXEL_FORMAT_1555;

    s32Ret = MI_RGN_GetAttr(*handle, &stRegionCurrent);

    if (s32Ret)
    {
        fprintf(stderr, "[%s:%d]RGN_GetAttr failed with %#x , creating region %d...\n", __func__, __LINE__, s32Ret,
                *handle);
        s32Ret = MI_RGN_Create(*handle, &stRegion);
        if (s32Ret)
        {
            fprintf(stderr, "[%s:%d]RGN_Create failed with %#x!\n", __func__, __LINE__, s32Ret);
            return -1;
        }
    }
    else
    {
        if (stRegionCurrent.stOsdInitParam.stSize.u32Height != stRegion.stOsdInitParam.stSize.u32Height ||
            stRegionCurrent.stOsdInitParam.stSize.u32Width != stRegion.stOsdInitParam.stSize.u32Width)

        {
            fprintf(stderr, "[%s:%d] Region parameters are different, recreating ... \n", __func__, __LINE__);
            stChn.s32OutputPortId = 1;
            MI_RGN_DetachFromChn(*handle, &stChn);
            stChn.s32OutputPortId = 0;
            MI_RGN_DetachFromChn(*handle, &stChn);
            MI_RGN_Destroy(*handle);
            s32Ret = MI_RGN_Create(*handle, &stRegion);

            if (s32Ret)
            {
                fprintf(stderr, "[%s:%d]RGN_Create failed with %#x!\n", __func__, __LINE__, s32Ret);
                return -1;
            }
        }
    }

    s32Ret = MI_RGN_GetDisplayAttr(*handle, &stChn, &stChnAttrCurrent);
    if (s32Ret)
        fprintf(stderr, "[%s:%d]RGN_GetDisplayAttr failed with %#x %d, attaching...\n", __func__, __LINE__, s32Ret,
                *handle);
    else if (stChnAttrCurrent.stPoint.u32X != x || stChnAttrCurrent.stPoint.u32Y != y)

    {
        fprintf(stderr, "[%s:%d] Position has changed, detaching handle %d from channel %d...\n", __func__, __LINE__,
                *handle, &stChn.s32ChnId);
        stChn.s32OutputPortId = 1;
        MI_RGN_DetachFromChn(*handle, &stChn);
        stChn.s32OutputPortId = 0;
        MI_RGN_DetachFromChn(*handle, &stChn);
    }

    memset(&stChnAttr, 0, sizeof(MI_RGN_ChnPortParam_t));
    stChnAttr.bShow = 1;
    stChnAttr.stPoint.u32X = x;
    stChnAttr.stPoint.u32Y = y;
    stChnAttr.unPara.stOsdChnPort.u32Layer = 0;
    stChnAttr.unPara.stOsdChnPort.stOsdAlphaAttr.eAlphaMode = E_MI_RGN_PIXEL_ALPHA;
    stChnAttr.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8BgAlpha = 0;
    stChnAttr.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8FgAlpha = 255;

    stChn.s32OutputPortId = 0;
    MI_RGN_AttachToChn(*handle, &stChn, &stChnAttr);
    stChn.s32OutputPortId = 1;
    MI_RGN_AttachToChn(*handle, &stChn, &stChnAttr);
    return s32Ret;
}

int prepare_bitmap(const char *filename, BITMAP *bitmap, int bFil, unsigned int u16FilColor, int enPixelFmt)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;
    int s32BytesPerPix = 2;
    unsigned char *pu8Data;
    int R_Value;
    int G_Value;
    int B_Value;
    int Gr_Value;
    unsigned char Value_tmp;
    unsigned char Value;
    int s32Width;

    if (parse_bitmap(filename, &bmpFileHeader, &bmpInfo) < 0)
    {
        fprintf(stderr, "GetBmpInfo err!\n");
        return -1;
    }

    switch (enPixelFmt)
    {
    case PIXEL_FORMAT_4444:
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
        break;
    case PIXEL_FORMAT_1555:
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
        break;
    case PIXEL_FORMAT_2BPP:
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
        break;
    case PIXEL_FORMAT_8888:
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
        s32BytesPerPix = 4;
        break;
    default:
        fprintf(stderr, "enPixelFormat err %d \n", enPixelFmt);
        return -1;
    }

    if (!(bitmap->pData = malloc(s32BytesPerPix * bmpInfo.bmiHeader.biWidth * bmpInfo.bmiHeader.biHeight)))
    {
        fputs("malloc osd memory err!\n", stderr);
        return -1;
    }

    CreateSurfaceByBitMap(filename, &Surface, (unsigned char *)(bitmap->pData));

    bitmap->u32Width = Surface.u16Width;
    bitmap->u32Height = Surface.u16Height;
    bitmap->enPixelFormat = enPixelFmt;

    int i, j, k;
    unsigned char *pu8Temp;

#ifndef __16CV300__
    if (enPixelFmt == PIXEL_FORMAT_2BPP)
    {
        s32Width = DIV_UP(bmpInfo.bmiHeader.biWidth, 4);
        pu8Data = malloc(s32Width * bmpInfo.bmiHeader.biHeight);
        if (!pu8Data)
        {
            fputs("malloc osd memory err!\n", stderr);
            return -1;
        }
    }

#endif
    if (enPixelFmt != PIXEL_FORMAT_2BPP)
    {
        unsigned short *pu16Temp;
        pu16Temp = (unsigned short *)bitmap->pData;
        if (bFil)
        {
            for (i = 0; i < bitmap->u32Height; i++)
            {
                for (j = 0; j < bitmap->u32Width; j++)
                {
                    if (u16FilColor == *pu16Temp)
                    {
                        *pu16Temp &= 0x7FFF;
                    }

                    pu16Temp++;
                }
            }
        }
    }
    else
    {
        unsigned short *pu16Temp;

        pu16Temp = (unsigned short *)bitmap->pData;
        pu8Temp = (unsigned char *)pu8Data;

        for (i = 0; i < bitmap->u32Height; i++)
        {
            for (j = 0; j < bitmap->u32Width / 4; j++)
            {
                Value = 0;

                for (k = j; k < j + 4; k++)
                {
                    B_Value = *pu16Temp & 0x001F;
                    G_Value = *pu16Temp >> 5 & 0x001F;
                    R_Value = *pu16Temp >> 10 & 0x001F;
                    pu16Temp++;

                    Gr_Value = (R_Value * 299 + G_Value * 587 + B_Value * 144 + 500) / 1000;
                    if (Gr_Value > 16)
                        Value_tmp = 0x01;
                    else
                        Value_tmp = 0x00;
                    Value = (Value << 2) + Value_tmp;
                }
                *pu8Temp = Value;
                pu8Temp++;
            }
        }
        free(bitmap->pData);
        bitmap->pData = pu8Data;
    }

    return 0;
}

int set_bitmap(int handle, BITMAP *bitmap)
{
    int s32Ret = MI_RGN_SetBitMap(handle, (MI_RGN_Bitmap_t *)(bitmap));

    if (s32Ret)
    {
        fprintf(stderr, "RGN_SetBitMap failed with %#x!\n", s32Ret);
        return -1;
    }
    return s32Ret;
}

void unload_region(int *handle)
{
    if (!handle || *handle < 0)
    {
        return;
    }

    MI_RGN_ChnPort_t stChn;
    stChn.eModId = E_MI_RGN_MODID_VPE;
    stChn.s32DevId = 0;
    stChn.s32ChnId = 0;

    stChn.s32OutputPortId = 1;
    MI_RGN_DetachFromChn(*handle, &stChn);
    stChn.s32OutputPortId = 0;
    MI_RGN_DetachFromChn(*handle, &stChn);

    MI_RGN_Destroy(*handle);
    *handle = -1;
}

static void fill(char *str)
{
    unsigned int rxb_l, txb_l, cpu_l[6];
    char out[DATA_SIZE] = "";
    char param = 0;
    int ipos = 0, opos = 0;

    while (str[ipos] != 0)
    {
        if (str[ipos] != '$')
        {
            strncat(out, str + ipos, 1);
            opos++;
        }
        else if (str[ipos + 1] == 'B')
        {
            ipos++;
            struct ifaddrs *ifaddr, *ifa;
            if (getifaddrs(&ifaddr) == -1)
                continue;

            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
            {
                if (equals(ifa->ifa_name, "lo"))
                    continue;
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_PACKET)
                    continue;
                if (!ifa->ifa_data)
                    continue;

                struct rtnl_link_stats *stats = ifa->ifa_data;
                char b[32];
                sprintf(b, "R:%dKbps S:%dKbps", (stats->rx_bytes - rxb_l) / 1024, (stats->tx_bytes - txb_l) / 1024);
                strcat(out, b);
                opos += strlen(b);
                rxb_l = stats->rx_bytes;
                txb_l = stats->tx_bytes;
                break;
            }

            freeifaddrs(ifaddr);
        }
        else if (str[ipos + 1] == 'C')
        {
            ipos++;
            char tmp[6];
            unsigned int cpu[6];
            FILE *stat = fopen("/proc/stat", "r");
            fscanf(stat, "%s %u %u %u %u %u %u", tmp, &cpu[0], &cpu[1], &cpu[2], &cpu[3], &cpu[4], &cpu[5]);
            fclose(stat);

            char c[5];
            char avg = 100 - (cpu[3] - cpu_l[3]) / sysconf(_SC_NPROCESSORS_ONLN);
            sprintf(c, "%d%%", avg);
            strcat(out, c);
            opos += strlen(c);
            for (int i = 0; i < sizeof(cpu) / sizeof(cpu[0]); i++)
                cpu_l[i] = cpu[i];
        }
        else if (str[ipos + 1] == 'M')
        {
            ipos++;
            struct sysinfo si;
            sysinfo(&si);

            char m[16];
            short used = (si.freeram + si.bufferram) / 1024 / 1024;
            short total = si.totalram / 1024 / 1024;
            sprintf(m, "%d/%dMB", used, total);
            strcat(out, m);
            opos += strlen(m);
        }
        else if (str[ipos + 1] == 't')
        {
            ipos++;
            char s[64];
            time_t t = time(NULL);
            struct tm *tm = gmtime(&t);
            strftime(s, 64, timefmt, tm);
            strcat(out, s);
            opos += strlen(s);
        }
        else if (str[ipos + 1] == '$')
        {
            ipos++;
            strcat(out, "$");
            opos++;
        }
        ipos++;
    }
    strncpy(str, out, DATA_SIZE);
}

void *region_thread()
{
    for (char id = 0; id < MAX_OSD; id++)
    {
        osds[id].hand = id;
        osds[id].color = DEF_COLOR;
        osds[id].opal = DEF_OPAL;
        osds[id].size = DEF_SIZE;
        osds[id].posx = DEF_POSX;
        osds[id].posy = DEF_POSY + (DEF_SIZE * 3 / 2) * id;
        osds[id].updt = 0;
        strcpy(osds[id].font, DEF_FONT);
        osds[id].text[0] = '\0';
    }
    while (keep_running)
    {
        for (int id = 0; id < MAX_OSD; id++)
        {
            char out[DATA_SIZE];
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            strftime(out, sizeof(out), timefmt, tm);
            strcat(out, osds[id].text);
            char *font;
            asprintf(&font, "/usr/share/fonts/truetype/%s.ttf", osds[id].font);
            if (!access(font, F_OK))
            {
                RECT rect = measure_text(font, osds[id].size, out);
                create_region(&osds[id].hand, osds[id].posx, osds[id].posy, rect.width, rect.height);
                BITMAP bitmap = raster_text(font, osds[id].size, out);
                if (bitmap.pData) {
                    set_bitmap(osds[id].hand, &bitmap);
                    free(bitmap.pData);
                    bitmap.pData = NULL;
                }
                if (font) {
                    free(font);
                    font = NULL;
                }
            }
        }
        sleep(1);
    }
}

int start_region_handler()
{
    printf("start_region_handler\n");
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 320 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
    {
        printf("[region] Can't set stack size %zu\n", new_stacksize);
    }
    pthread_create(&regionPid, &thread_attr, (void *(*)(void *))region_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
    {
        printf("[region] Error:  Can't set stack size %zu\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}

void stop_region_handler()
{
    pthread_join(regionPid, NULL);
}
