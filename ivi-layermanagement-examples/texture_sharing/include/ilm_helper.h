/**
 * \file: window.h
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
#ifndef ILM_HELPER_H
#define ILM_HELPER_H

#include <stdio.h>

#include <wayland-client.h>
#include "ilm_client.h"

typedef struct _ilm_params
{
    t_ilm_surface surface_id;
    t_ilm_layer   layer_id;
    t_ilm_uint    pid;
    char          window_title[64];
    t_ilm_uint    surface_width;
    t_ilm_uint    surface_height;
    t_ilm_int     layer_dest_x;
    t_ilm_int     layer_dest_y;
    t_ilm_uint    layer_width;
    t_ilm_uint    layer_height;
    struct wl_surface   *nativehandle;
    struct wl_display    *wlDisplay;
} ilm_params;

typedef enum
{
    RENDER_ORDER_FRONT  = 0x1,
    RENDER_ORDER_BEHIND = 0x2
} RENDER_ORDER_TYPE;

typedef enum
{
    RENDER_ORDER_CLEAR = 0x10000,
    RENDER_ORDER_ADD   = 0x20000
} RENDER_ORDER_METHOD;

/** Function prototypes */
int create_ilm_context(ilm_params *p_param);
int destroy_ilm_context(ilm_params *p_param);
int display_layer(t_ilm_int screen_id, t_ilm_layer layer_id, unsigned int render_order_flag);

/** Log macro */
#define ILM_error(...) {                         \
    fprintf(stderr, "ILM ERROR : " __VA_ARGS__); \
}

#define ILM_info(...) {                          \
    fprintf(stdout, "ILM INFO  : " __VA_ARGS__); \
}

#define ILM_debug(...) {                             \
    if (g_verbose)                                   \
    {                                                \
        fprintf(stdout, "ILM DEBUG : " __VA_ARGS__); \
    }                                                \
}

#endif
