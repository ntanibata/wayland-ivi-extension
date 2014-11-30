/**
 * \file: texture-sharing.c
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
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "texture_sharing.h"
#include "texture_sharing_test.h"
#include "bitmap.h"
#include "touch_event_util.h"

#include "ilm_helper.h"

#define WINDOW_TITLE     "texture-sharing"
#define DEFAULT_SAVE_DIR "/tmp"
#define SCREENSHOT_FILE  "texture-sharing.bmp"
#ifndef PATH_MAX
#  define PATH_MAX         1024
#endif

static const char *vert_shader_text =
    "uniform mediump float uX;                    \n"
    "uniform mediump float uY;                    \n"
    "uniform mediump float uWidth;                \n"
    "uniform mediump float uHeight;               \n"
    "attribute vec4 pos;                          \n"
    "attribute vec2 tex_pos;                      \n"
    "varying mediump vec2 v_tex_pos;              \n"
    "void main() {                                \n"
    "    mediump vec4 position;                   \n"
    "    position.xy = vec2(uX + uWidth  * pos.x, \n"
    "                       uY + uHeight * pos.y);\n"
    "    position.xy = 2.0 * position.xy - 1.0;   \n"
    "    position.zw = vec2(0.0, 1.0);            \n"
    "    gl_Position = position;                  \n"
    "    v_tex_pos = tex_pos;                     \n"
    "}                                            \n";

static const char *frag_shader_text =
    "precision mediump float;                              \n"
    "uniform mediump sampler2D u_tex_unit;                 \n"
    "varying mediump vec2 v_tex_pos;                       \n"
    "void main() {                                         \n"
    "    gl_FragColor = texture2D(u_tex_unit, v_tex_pos); \n"
    "}                                                     \n";

static PFNEGLCREATEIMAGEKHRPROC            pfEglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC           pfEglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pfGLEglImageTargetTexture2DOES;

static struct TextureSharing *gp_texture_sharing;
static ilm_params g_ilm_param;

static int      g_get_or_destroy = 0;
static uint32_t g_start_time = 0;

static int g_n_fail;
static struct event_log_array g_log_array;

/******************************************************************************/

void
share_surface_configure(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t type,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    _UNUSED_(p_share_surf);
    _UNUSED_(type);
    _UNUSED_(format);

    struct ShareSurfaceInfo *p_info = (struct ShareSurfaceInfo*)p_data;

    if ((p_info->sharing_buffer.id != 0) &&
        (p_info->sharing_buffer.id != id))
    {
        return;
    }

    p_info->sharing_buffer.id     = id;
    p_info->sharing_buffer.width  = width;
    p_info->sharing_buffer.height = height;
    p_info->sharing_buffer.stride = stride;
}

void
share_surface_update(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t name)
{
    _UNUSED_(p_share_surf);

    struct ShareSurfaceInfo *p_info = (struct ShareSurfaceInfo*)p_data;

    if ((p_info->sharing_buffer.id == 0) ||
        (p_info->sharing_buffer.id != id))
    {
        return;
    }

    if (0 >= name)
    {
        return;
    }

    if (NULL != p_info->sharing_buffer.eglimage)
    {
        pfEglDestroyImageKHR(p_info->p_share->p_display->egldisplay,
            p_info->sharing_buffer.eglimage);
    }
    p_info->sharing_buffer.eglimage = NULL;

    {
        EGLint img_attribs[] = {
            EGL_WIDTH,                  p_info->sharing_buffer.width,
            EGL_HEIGHT,                 p_info->sharing_buffer.height,
            EGL_DRM_BUFFER_STRIDE_MESA, p_info->sharing_buffer.stride,
            EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
            EGL_NONE
        };

        p_info->sharing_buffer.eglimage = pfEglCreateImageKHR(
            p_info->p_share->p_display->egldisplay,
            EGL_NO_CONTEXT, EGL_DRM_BUFFER_MESA, (EGLClientBuffer)name,
            img_attribs);
        if (NULL == p_info->sharing_buffer.eglimage)
        {
            fprintf(stderr, "[ERR] failed to create eglImage: 0x%04x\n",
                eglGetError());
        }
    }
}

static void
share_surface_input_capabilities(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t caps)
{
    _UNUSED_(p_share_surf);

    struct ShareSurfaceInfo *p_info = (struct ShareSurfaceInfo*)p_data;

    if (NULL != p_info)
    {
        if (caps & WL_SHARE_SURFACE_EXT_INPUT_CAPS_TOUCH)
        {
            p_info->enable_touch_event_redirection = 1;
        }
        else
        {
            fprintf(stderr, "[WARNING] The share_surface does not have\n"
                            "          the capability to recive a touch event.\n");
            p_info->enable_touch_event_redirection = 0;
        }
    }
}

static const
struct wl_share_surface_ext_listener default_share_surface_listener = {
    share_surface_update,
    share_surface_configure,
    share_surface_input_capabilities
};

static void take_snapshot_of_surface(struct TextureSharing *p_share);
static void get_share_surface(struct TextureSharing *p_share);
static void destroy_share_surface(struct TextureSharing *p_share);

static void
frame_callback(void *p_data, struct wl_callback *p_cb, uint32_t time)
{
    struct TextureSharing *p_share =
        ((struct WaylandEglWindow*)p_data)->p_user_data;

    if ((NULL != p_share) && (1 == p_share->repeat_get_and_destroy))
    {
        if (0 == g_start_time)
        {
            g_start_time = time;
        }
        else
        {
            if (5000 < (time - g_start_time))
            {
                if (g_get_or_destroy == 0)
                {
                    destroy_share_surface(p_share);
                }
                else
                {
                    get_share_surface(p_share);
                }
                g_get_or_destroy = !g_get_or_destroy;
                g_start_time = 0;
            }
        }
    }

    WindowScheduleRedraw((struct WaylandEglWindow*)p_data);

    if (NULL != p_cb)
        wl_callback_destroy(p_cb);
}

static const struct wl_callback_listener frame_listener = {
    frame_callback
};

static void
update_log(uint32_t event, int32_t id, float x, float y, uint32_t time)
{
    printf("[%s]-[%s]\n", __FILE__, __func__);
    struct event_log l = {
        .event = event,
        .id    = id,
        .x     = x,
        .y     = y,
        .time  = time
    };
    log_array_add(&g_log_array, &l);
}

static void
touch_handle_down(void *p_data, struct wl_touch *p_touch, uint32_t serial,
    uint32_t time, struct wl_surface *p_surface, int32_t id,
    wl_fixed_t x_w, wl_fixed_t y_w)
{
    _UNUSED_(p_touch);
    _UNUSED_(p_surface);

    struct TextureSharing *p_share = (struct TextureSharing*)p_data;
    struct ShareSurfaceInfo *p_info;

    if (!p_share && p_share->p_touch)
    {
        return;
    }

    wl_list_for_each(p_info, &p_share->share_surface_list, link)
    {
        if (p_info->enable_touch_event_redirection && p_info->p_share_surf)
        {
            wl_share_surface_ext_redirect_touch_down(p_info->p_share_surf,
                serial, id, x_w, y_w);
        }
    }

    update_log(TOUCH_DOWN, id,
        (float)wl_fixed_to_double(x_w), (float)wl_fixed_to_double(y_w), time);
}

static void
touch_handle_up(void *p_data, struct wl_touch *p_touch, uint32_t serial,
    uint32_t time, int32_t id)
{
    _UNUSED_(p_touch);

    struct TextureSharing *p_share = (struct TextureSharing*)p_data;
    struct ShareSurfaceInfo *p_info;

    if (!p_share && p_share->p_touch)
    {
        return;
    }

    wl_list_for_each(p_info, &p_share->share_surface_list, link)
    {
        if (p_info->enable_touch_event_redirection && p_info->p_share_surf)
        {
            wl_share_surface_ext_redirect_touch_up(p_info->p_share_surf,
                serial, id);
        }
    }

    update_log(TOUCH_UP, id, 0.0f, 0.0f, time);
}

static void
touch_handle_motion(void *p_data, struct wl_touch *p_touch, uint32_t time,
    int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    _UNUSED_(p_touch);

    struct TextureSharing *p_share = (struct TextureSharing*)p_data;
    struct ShareSurfaceInfo *p_info;

    if (!p_share && p_share->p_touch)
    {
        return;
    }

    wl_list_for_each(p_info, &p_share->share_surface_list, link)
    {
        if (p_info->enable_touch_event_redirection && p_info->p_share_surf)
        {
            wl_share_surface_ext_redirect_touch_motion(p_info->p_share_surf,
                id, x_w, y_w);
        }
    }

    update_log(TOUCH_MOTION, id,
        (float)wl_fixed_to_double(x_w), (float)wl_fixed_to_double(y_w), time);
}

static void
touch_handle_frame(void *p_data, struct wl_touch *p_touch)
{
    _UNUSED_(p_data);
    _UNUSED_(p_touch);
}

static void
touch_handle_cancel(void *p_data, struct wl_touch *p_touch)
{
    _UNUSED_(p_data);
    _UNUSED_(p_touch);
}

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel
};

static void
seat_handle_capabilities(void *p_data, struct wl_seat *p_seat,
    enum wl_seat_capability caps)
{
    struct TextureSharing *p_share = (struct TextureSharing*)p_data;

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !p_share->p_touch)
    {
        p_share->p_touch = wl_seat_get_touch(p_seat);
        wl_touch_set_user_data(p_share->p_touch, p_data);
        wl_touch_add_listener(p_share->p_touch, &touch_listener, p_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && p_share->p_touch)
    {
        wl_touch_destroy(p_share->p_touch);
        p_share->p_touch = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities
};

/******************************************************************************/

static void
display_global_handler(struct WaylandDisplay *p_display, uint32_t name,
    const char *p_interface, uint32_t version, void *p_data)
{
    _UNUSED_(version);

    struct TextureSharing *p_share = (struct TextureSharing*)p_data;

    if (0 == strcmp(p_interface, "wl_share_ext"))
    {
        p_share->p_share_ext = wl_registry_bind(p_display->p_registry,
            name, &wl_share_ext_interface, (1 < version) ? 2 : 1);
        fprintf(stderr, "[INFO] wl_share_ext interface version [%d]\n",
            version);
    }
    else if (0 == strcmp(p_interface, "wl_seat"))
    {
        if (1 == p_share->bind_seat)
        {
            p_share->p_seat = wl_registry_bind(p_display->p_registry,
                name, &wl_seat_interface, 1);
            wl_seat_add_listener(p_share->p_seat, &seat_listener, p_data);
        }
    }
}

static GLuint
create_shader(const char *p_source, GLenum shader_type)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(shader_type);
    assert(shader != 0);

    glShaderSource(shader, 1, (const char**)&p_source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (0 == status)
    {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        fprintf(stderr, "[ERR] compiling %s:\n%*s\n",
            shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment", len, log);
        exit(-1);
    }

    return shader;
}

static void
init_gl(struct TextureSharing *p_share)
{
    GLuint vert, frag;
    GLuint program;
    GLint status;

    pfGLEglImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    pfEglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
        eglGetProcAddress("eglCreateImageKHR");
    pfEglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
        eglGetProcAddress("eglDestroyImageKHR");

    assert(pfGLEglImageTargetTexture2DOES
        && pfEglCreateImageKHR
        && pfEglDestroyImageKHR);

    vert = create_shader(vert_shader_text, GL_VERTEX_SHADER);
    frag = create_shader(frag_shader_text, GL_FRAGMENT_SHADER);

    program = glCreateProgram();
    glAttachShader(program, frag);
    glAttachShader(program, vert);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (0 == status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, sizeof(log), &len, log);
        fprintf(stderr, "[ERR] linking:\n%s\n", log);
        exit(-1);
    }

    glUseProgram(program);

    p_share->gl.pos = 0;
    p_share->gl.tex_pos = 1;

    glBindAttribLocation(program, p_share->gl.pos, "pos");
    glBindAttribLocation(program, p_share->gl.tex_pos, "tex_pos");
    glLinkProgram(program);

    p_share->gl.loc_x = glGetUniformLocation(program, "uX");
    p_share->gl.loc_y = glGetUniformLocation(program, "uY");
    p_share->gl.loc_w = glGetUniformLocation(program, "uWidth");
    p_share->gl.loc_h = glGetUniformLocation(program, "uHeight");
}

static void
get_share_surface(struct TextureSharing *p_share)
{
    struct ShareSurfaceInfo *p_info;

    wl_list_for_each(p_info, &p_share->share_surface_list, link)
    {
        p_info->p_share_surf = wl_share_ext_get_share_surface(
            p_share->p_share_ext, p_info->pid, p_info->p_window_title);

        if (1 == p_share->enable_listener)
        {
            wl_share_surface_ext_add_listener(p_info->p_share_surf,
                &p_share->share_surface_listener, p_info);
        }
        else
        {
            wl_share_surface_ext_add_listener(p_info->p_share_surf,
                &default_share_surface_listener, p_info);
        }
    }
}

static void
destroy_share_surface(struct TextureSharing *p_share)
{
    struct ShareSurfaceInfo *p_info;

    wl_list_for_each(p_info, &p_share->share_surface_list, link)
    {
        wl_share_surface_ext_destroy(p_info->p_share_surf);
        p_info->p_share_surf = NULL;
    }
}

static int
create_texture_sharing(struct TextureSharing *p_share)
{
    init_gl(p_share);

    get_share_surface(p_share);

    WindowScheduleResize(p_share->p_window, p_share->width, p_share->height);

    return 0;
}

static void
take_snapshot_of_surface(struct TextureSharing *p_share)
{
    int rc;
    int row, col, offset;
    char *p_rgb = NULL;
    char *p_buffer = (char*)
        malloc(p_share->width * p_share->height * 4 * sizeof(char));
    int pending = 0;
    int index = 0;
    int image_size = 0;
    int i;
    char image_file[PATH_MAX];

    glReadPixels(0, 0, p_share->width, p_share->height, GL_RGBA,
        GL_UNSIGNED_BYTE, p_buffer);
    rc = glGetError();
    if (rc != GL_NO_ERROR)
    {
        fprintf(stderr, "reading pixel for snapshot failed: %d\n", rc);
        free(p_buffer);
        return;
    }

    pending = (p_share->width * 3) % 4;
    image_size = (p_share->width * p_share->height * 3) + (p_share->height * pending);

    p_rgb = (char*)malloc(image_size);

    for (row = 0; row < p_share->height; ++row)
    {
        for (col = 0; col < p_share->width; ++col)
        {
            offset = row * p_share->width + col;
            p_rgb[index++] = p_buffer[offset*4+2];
            p_rgb[index++] = p_buffer[offset*4+1];
            p_rgb[index++] = p_buffer[offset*4  ];
        }

        for (i = 0; i < pending; ++i)
        {
            p_rgb[index++] = 0;
        }
    }

    sprintf(image_file, "%s/%s",
        (NULL != p_share->p_tmp_dir) ? p_share->p_tmp_dir : DEFAULT_SAVE_DIR,
        SCREENSHOT_FILE);

    TEST_info("Output snapshot - image_file (%s)\n", image_file);
    if (0 != write_bitmap(image_file, image_size, p_rgb,
                          p_share->width, p_share->height))
    {
        TEST_error("Failed to write image file\n");
    }

    free(p_buffer);
    free(p_rgb);
}

static const GLfloat tex_coords[4][2] = {
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 0, 0 },
};

static const GLfloat g_vertices[4][2] = {
    {0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0},
    {0.0, 1.0}
};

static uint32_t g_frame;

static void
redraw_handler(struct WaylandEglWindow *p_window, void *p_data)
{
    struct TextureSharing *p_share = (struct TextureSharing *)p_data;
    struct ShareSurfaceInfo *p_surface_info;
    struct wl_callback *p_cb;
    GLfloat w, h, uX, uY;
    GLfloat dest_x      = 0.0;
    GLfloat dest_y      = 0.0;
    GLfloat dest_width  = 0.0;
    GLfloat dest_height = 0.0;;

    DisplayAcquireWindowSurface(p_window->p_display, p_window);

    glViewport(0, 0, p_window->geometry.width, p_window->geometry.height);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(p_share->gl.pos);
    glEnableVertexAttribArray(p_share->gl.tex_pos);

    wl_list_for_each(p_surface_info, &p_share->share_surface_list, link)
    {
        w = p_surface_info->sharing_buffer.width;
        h = p_surface_info->sharing_buffer.height;

        uX = dest_x / p_share->width;
        uY = 1.0f - (dest_y + h) / p_share->height;
        dest_width  = w / p_share->width;
        dest_height = h / p_share->height;

        glUniform1fv(p_share->gl.loc_x, 1, &uX);
        glUniform1fv(p_share->gl.loc_y, 1, &uY);
        glUniform1fv(p_share->gl.loc_w, 1, &dest_width);
        glUniform1fv(p_share->gl.loc_h, 1, &dest_height);

        if (0 == p_surface_info->sharing_buffer.tex_obj_id)
        {
            glGenTextures(1, &p_surface_info->sharing_buffer.tex_obj_id);
            glBindTexture(GL_TEXTURE_2D,
                p_surface_info->sharing_buffer.tex_obj_id);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D,
                p_surface_info->sharing_buffer.tex_obj_id);
        }

        if (NULL != p_surface_info->sharing_buffer.eglimage)
        {
            pfGLEglImageTargetTexture2DOES(
                GL_TEXTURE_2D, p_surface_info->sharing_buffer.eglimage);
        }

        glVertexAttribPointer(
            p_share->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, g_vertices);
        glVertexAttribPointer(
            p_share->gl.tex_pos, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        if ((dest_x + w) > (p_share->width - w))
        {
            dest_x  = 0.0;
            dest_y += h;
        }
        else
        {
            dest_x += w;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(p_share->gl.pos);
    glDisableVertexAttribArray(p_share->gl.tex_pos);

    if (1 == p_share->take_snapshot)
    {
        if (100 < ++g_frame)
        {
            glFinish();
            take_snapshot_of_surface(p_share);
            p_share->take_snapshot = 0;
        }
    }

    p_cb = wl_surface_frame(p_window->p_surface);
    wl_callback_add_listener(p_cb, &frame_listener, p_window);
}

static int
compare_log(struct event_log *p_log1, struct event_log *p_log2)
{
    float eps = 1.0e-3;

    if (p_log1->event != p_log2->event)
    {
        return 1;
    }

    if (p_log1->id != p_log2->id)
    {
        return 1;
    }

    if (eps < fabsf(p_log1->x - p_log2->x))
    {
        return 1;
    }

    if (eps < fabsf(p_log1->y - p_log2->y))
    {
        return 1;
    }

    return 0;
}

void
texture_sharing_evaluate_event_log()
{
    char logfile[256];
    FILE *p_fp = NULL;
    size_t n_log = 0;
    struct event_log *p_logs = NULL;
    int n_fail = 0;
    size_t i;

    printf("** Evaluate touch event log *********\n");

    sprintf(logfile, "%s/touch_event_log.bin", gp_texture_sharing->p_tmp_dir);

    if (NULL == (p_fp = fopen(logfile, "rb")))
    {
        LOG_ERROR("File open failed [%s]\n", logfile);
        goto err_rtn;
    }

    fread(&n_log, sizeof(int), 1, p_fp);
    printf(" Number of touch event\n"
           "   Transmitted touch event : %d\n"
           "      Received touch event : %d\n", (int)n_log, (int)g_log_array.n_log);
    if (n_log != g_log_array.n_log)
    {
        LOG_ERROR("There's a discrepancy between the two logs\n");
        goto err_rtn;
    }

    p_logs = (struct event_log*)malloc(sizeof(struct event_log) * n_log);
    if (NULL == p_logs)
    {
        LOG_ERROR("Memory allocation failed\n");
        goto err_rtn;
    }

    fread(p_logs, sizeof(struct event_log), n_log, p_fp);

    for (i = 0; i < n_log; ++i)
    {
        if (0 != compare_log(&p_logs[i], &g_log_array.p_logs[i]))
        {
            printf(" Difference of event: %d\n", (int)i + 1);
            printf("            ev   id       x         y\n");
            printf("    sender:  %d    %d    %7.3f   %7.3f\n",
                p_logs[i].event,
                p_logs[i].id,
                p_logs[i].x,
                p_logs[i].y);
            printf("  receiver:  %d    %d    %7.3f   %7.3f\n",
                g_log_array.p_logs[i].event,
                g_log_array.p_logs[i].id,
                g_log_array.p_logs[i].x,
                g_log_array.p_logs[i].y);
            printf("\n");
            if (10 < ++n_fail)
            {
                printf(" The differences exceeded ten, stop.\n");
                break;
            }
        }
    }

    if (0 < n_fail)
    {
        goto err_rtn;
    }

    goto rtn;

err_rtn:
    ++g_n_fail;

rtn:
    if (NULL != p_logs)
    {
        free(p_logs);
    }
    if (NULL != p_fp)
    {
        fclose(p_fp);
    }
    printf("** Done *****************************\n");

    /* reset logs */
    log_array_release(&g_log_array);
    log_array_init(&g_log_array, 1000);

    return;
}

void
texture_sharing_terminate()
{
    if (NULL != gp_texture_sharing)
    {
        DisplayExit(gp_texture_sharing->p_display);
    }
    else
    {
        exit(EXIT_FAILURE);
    }
}

int
texture_sharing_main(struct TextureSharing *p_ts)
{
    struct WaylandDisplay   *p_display;
    struct WaylandEglWindow *p_window;
    int rc;

    if (NULL == p_ts)
    {
        return -1;
    }

    /* Save global memory */
    gp_texture_sharing = p_ts;

    /**
     * Create Display
     */
    p_display = CreateDisplay(0, NULL);
    if (NULL == p_display)
    {
        fprintf(stderr, "CreateDisplay failed");
        return -1;
    }
    p_ts->p_display = p_display;

    DisplaySetUserData(p_display, p_ts);
    DisplaySetGlobalHandler(p_display, display_global_handler);

    /* initialize log array */
    if (1 == p_ts->bind_seat)
    {
        log_array_init(&g_log_array, 1000);
    }

    /**
     * Create Window
     */
    p_window = CreateEglWindow(p_display, WINDOW_TITLE);
    if ((NULL == p_window) || (0 != wl_list_empty(&p_display->surface_list)))
    {
        fprintf(stderr, "CreateEglWindow failed");
        DestroyDisplay(p_display);
        return -1;
    }
    p_ts->p_window = p_window;

    /* Set redraw handler & user data */
    p_window->redraw_handler = redraw_handler;
    p_window->p_user_data = p_ts;

    /**
     * Create Drawing
     */
    if (0 > create_texture_sharing(p_ts))
    {
        fprintf(stderr, "create_texture_sharing failed");
        DestroyDisplay(p_display);
        return -1;
    }

    /* Force roundtrip */
    wl_display_roundtrip(p_display->p_display);

    memset(&g_ilm_param, 0x00, sizeof(ilm_params));
    g_ilm_param.surface_id     = p_ts->surface_id;
    g_ilm_param.layer_id       = p_ts->layer_id;
    g_ilm_param.pid            = getpid();
    g_ilm_param.surface_width  = p_ts->width;
    g_ilm_param.surface_height = p_ts->height;
    g_ilm_param.layer_dest_x   = p_ts->x;
    g_ilm_param.layer_dest_y   = p_ts->y;
    g_ilm_param.layer_width    = p_ts->width;
    g_ilm_param.layer_height   = p_ts->height;
    g_ilm_param.nativehandle   = p_window->p_surface;
    g_ilm_param.wlDisplay      = p_window->p_display->p_display;
    strcpy(g_ilm_param.window_title, WINDOW_TITLE);

    do
    {
        /* Create ILM Surface and ILM Layer */
        rc = create_ilm_context(&g_ilm_param);
        if (0 > rc)
        {
            fprintf(stderr, "create_ilm_context failed\n");
            break;
        }
        fprintf(stderr, "create_ilm_context comp\n");
        /* Display Layer on screen 0 */
        rc = display_layer(0, g_ilm_param.layer_id,
            RENDER_ORDER_FRONT | RENDER_ORDER_ADD);
        fprintf(stderr, "display_layer comp\n");
        if (0 > rc)
        {
            fprintf(stderr, "display_layer failed\n");
            break;
        }

    } while (0);

    if (0 != rc)
    {
        fprintf(stderr, "0 != rc\n");
        return -1;
    }

    /**
     * Show & Handle Events
     */
    DisplayRun(p_display);

    /* Cleanup */
    destroy_ilm_context(&g_ilm_param);

    DestroyDisplay(p_display);

    /* destroy log array */
    if (1 == p_ts->bind_seat)
    {
        log_array_release(&g_log_array);
    }

    return g_n_fail;
}

#ifndef USE_LTP
int
main(int argc, char **argv)
{
    struct TextureSharing ts;
    struct ShareSurfaceInfo *p_info;

    if (argc != 3)
    {
        fprintf(stderr, "usage: texture_sharing_test <pid to share> <window title to share>\n");
        return 1;
    }

    memset(&ts, 0x00, sizeof(ts));
    wl_list_init(&ts.share_surface_list);

    ts.width  = 250;
    ts.height = 250;
    ts.surface_id = 10000;
    ts.layer_id   = 10000;

    p_info = (struct ShareSurfaceInfo*)calloc(1, sizeof(*p_info));

    p_info->p_share = &ts;
    p_info->pid     = atoi(argv[1]);
    p_info->p_window_title = strdup(argv[2]);
    wl_list_insert(ts.share_surface_list.prev, &p_info->link);

    return texture_sharing_main(&ts);
}
#endif
