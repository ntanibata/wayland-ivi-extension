/**
 * \file: bitmap.c
 *
 * \version: $Id:$
 *
 * \release: $Name:$
 *
 * <brief description>.
 * <detailed description>
 * \component: <componentname>
 *
 * \author: <author>
 *
 * \copyright (c) 2012, 2013 Advanced Driver Information Technology.
 * This code is developed by Advanced Driver Information Technology.
 * Copyright of Advanced Driver Information Technology, Bosch, and DENSO.
 * All rights reserved.
 *
 * \see <related items>
 *
 * \history
 * <history item>
 * <history item>
 * <history item>
 *
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _BITMAPFILEHEADER {
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;
} BITMAPFILEHEADER;

typedef struct _BITMAPINFOHEADER {
    unsigned int   biSize;
    int            biWidth;
    int            biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int   biCompression;
    unsigned int   biSizeImage;
    int            biXPixPerMeter;
    int            biYPixPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImporant;
} BITMAPINFOHEADER;

/**
 * \func   write_bitmap
 *
 * \param  p_rgb:
 * \param  width:
 * \param  height:
 *
 * \return int: return status
 *
 * \see
 */
int
write_bitmap(const char *p_path, int image_size, const char *p_rgb, int width, int height)
{
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;
    char magic_number[2] = {'B','M'};
    FILE *p_fp;

    memset(&fileHeader, 0x00, sizeof(BITMAPFILEHEADER));
    memset(&infoHeader, 0x00, sizeof(BITMAPINFOHEADER));

    /* [2] is size of magic number */
    fileHeader.bfSize    = 2 + sizeof(fileHeader) + sizeof(infoHeader) + image_size;
    fileHeader.bfOffBits = 2 + sizeof(fileHeader) + sizeof(infoHeader);

    infoHeader.biSize      = sizeof(infoHeader);
    infoHeader.biWidth     = width;
    infoHeader.biHeight    = height;
    infoHeader.biPlanes    = 1;
    infoHeader.biBitCount  = 24;
    infoHeader.biSizeImage = image_size;

    p_fp = fopen(p_path, "wb");
    if (NULL == p_fp)
    {
        return 1;
    }

    fwrite(magic_number, 2, 1, p_fp);
    fwrite((void*)&fileHeader, sizeof(fileHeader), 1, p_fp);
    fwrite((void*)&infoHeader, sizeof(infoHeader), 1, p_fp);
    fwrite(p_rgb, image_size, 1, p_fp);

    fclose(p_fp);

    return 0;
}
