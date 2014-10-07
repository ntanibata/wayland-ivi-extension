/**
 * \file: touch_event_test.h
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
#ifndef TOUCH_EVENT_TEST_H
#define TOUCH_EVENT_TEST_H

#include "window.h"
#include "touch_event_util.h"

struct touch_point_params
{
    struct wl_list link;
    int            display;
    int            id;
    float          x, y;
    float          r;
    int32_t        n_vtx;
    GLfloat       *p_vertices;
    GLfloat        color[3];
};

struct touch_event_test_params
{
    struct WaylandDisplay   *p_display;
    struct WaylandEglWindow *p_window;
    struct wl_seat          *p_seat;
    struct wl_touch         *p_touch;
    struct wl_list           touch_point_list;
    struct event_log_array   log_array;
    char                    *p_logfile;
    int                      n_fail;

    struct {
        GLuint pos;
        GLuint loc_col;
        GLuint loc_x;
        GLuint loc_y;
        GLuint loc_w;
        GLuint loc_h;
    } gl;
};

#endif /* TOUCH_EVENT_TEST_H */
