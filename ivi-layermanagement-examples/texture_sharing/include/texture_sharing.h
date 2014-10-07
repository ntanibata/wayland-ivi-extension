/**
 * \file: texture_sharing.h
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
#ifndef TEXTURE_SHARING_H
#define TEXTURE_SHARING_H

#include <sys/types.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "window.h"
#include "ivi-share-extension-client-protocol.h"

struct SharingBuffer
{
    uint32_t       id;
    uint32_t       width;
    uint32_t       height;
    uint32_t       stride;
    EGLImageKHR    eglimage;
    GLuint         tex_obj_id;
};

struct ShareSurfaceInfo
{
    struct TextureSharing       *p_share;
    pid_t                        pid;
    char                        *p_window_title;
    struct wl_share_surface_ext *p_share_surf;
    struct SharingBuffer         sharing_buffer;
    int                          enable_touch_event_redirection;
    struct wl_list               link;
};

struct TextureSharing
{
    struct wl_share_ext     *p_share_ext;
    struct wl_seat          *p_seat;
    struct wl_touch         *p_touch;
    struct WaylandDisplay   *p_display;
    struct WaylandEglWindow *p_window;
    int32_t                  x, y;
    int32_t                  width;
    int32_t                  height;
    uint32_t                 layer_id;
    uint32_t                 surface_id;
    struct {
        GLuint pos;
        GLuint tex_pos;
        GLuint loc_x;
        GLuint loc_y;
        GLuint loc_w;
        GLuint loc_h;
    } gl;
    char                    *p_tmp_dir;
    int                      repeat_get_and_destroy;
    int                      take_snapshot;
    int                      enable_listener;
    int                      bind_seat;
    struct wl_share_surface_ext_listener
                             share_surface_listener;
    struct wl_list           share_surface_list;
};

int texture_sharing_main(struct TextureSharing *p_ts);
void texture_sharing_evaluate_event_log(void);
void texture_sharing_terminate();
void share_surface_configure(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t type,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t format);
void share_surface_update(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t name);

#endif
