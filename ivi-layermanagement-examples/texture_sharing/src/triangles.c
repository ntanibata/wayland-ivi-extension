/**
 * \file: window.c
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
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <GLES2/gl2.h>
#include "window.h"
#include "bitmap.h"

static const char *VertexShaderText =
    "uniform mat4 rotation; \n"
    "attribute vec4 pos;    \n"
    "attribute vec4 color;  \n"
    "varying vec4 v_color;  \n"
    "void main() {          \n"
    "    gl_Position = rotation * pos; \n"
    "    v_color = color;              \n"
    "}                                 \n";

static const char *FragShaderText =
    "precision mediump float;    \n"
    "varying vec4 v_color;       \n"
    "void main() {               \n"
    "    gl_FragColor = v_color; \n"
    "}                           \n";

#define PATH_MAX        1024
#define WINDOW_WIDTH	250
#define WINDOW_HEIGHT	250
#define FORCE_REDRAW	1
#define DEFAULT_SAVE_DIR "/tmp"
#define SCREENSHOT_FILE  "triangles.bmp"

/* macros for logging */
#ifdef LOG_ERROR
#  undef LOG_ERROR
#endif
#define LOG_ERROR(...) {                               \
    fprintf(stderr, "triangles ERROR : " __VA_ARGS__); \
}

#ifdef LOG_INFO
#  undef LOG_INFO
#endif
#define LOG_INFO(...) {                                \
    fprintf(stdout, "triangles INFO  : " __VA_ARGS__); \
}

#ifdef LOG_DEBUG
#  undef LOG_DEBUG
#endif
#define LOG_DEBUG(...) {                               \
    fprintf(stdout, "triangles DEBUG : " __VA_ARGS__); \
}

static struct WaylandDisplay *gp_display = NULL;
static int g_rotation = 1;
static int g_snapshot = 0;
static int g_frame = 0;
static char g_save_dir[PATH_MAX];

/******************************************************************************/
static void
frame_callback(void *p_data, struct wl_callback *p_cb, uint32_t time)
{
    _UNUSED_(time);

    WindowScheduleRedraw((struct WaylandEglWindow*)p_data);

    if (NULL != p_cb)
        wl_callback_destroy(p_cb);
}

static const struct wl_callback_listener frame_listener = {
    frame_callback
};

static GLuint
CreateShader(const char *p_source, GLenum shader_type)
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
        LOG_ERROR("[ERR] compiling %s:\n%*s\n",
            shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment", len, log);
        exit(-1);
    }

    return shader;
}

static void
InitGL(struct wl_list *p_window_list)
{
    GLuint frag, vert;
    GLuint program;
    GLint status;
    GLuint pos, col, rotation_uniform;
    struct WaylandEglWindow *p_window;

    vert = CreateShader(VertexShaderText, GL_VERTEX_SHADER);
    frag = CreateShader(FragShaderText, GL_FRAGMENT_SHADER);

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
        LOG_ERROR("[ERR] linking:\n%*s\n", len, log);
        exit(-1);
    }

    glUseProgram(program);
    pos = 0;
    col = 1;
    glBindAttribLocation(program, pos, "pos");
    glBindAttribLocation(program, col, "color");
    glLinkProgram(program);

    rotation_uniform = glGetUniformLocation(program, "rotation");

    wl_list_for_each(p_window, p_window_list, link)
    {
        p_window->gl.pos = pos;
        p_window->gl.col = col;
        p_window->gl.rotation_uniform = rotation_uniform;
    }
}

static void
save_screenshot_of_window(struct WaylandEglWindow *p_window)
{
    int rc;
    int row, col, offset;
    char *p_rgb = NULL;
    char *p_buffer = (char*)
        malloc(p_window->geometry.width * p_window->geometry.height * 4 * sizeof(char));
    int pending = 0;
    int index = 0;
    int image_size = 0;
    int i;
    char image_file[PATH_MAX];

    glReadPixels(0, 0, p_window->geometry.width, p_window->geometry.height, GL_RGBA,
        GL_UNSIGNED_BYTE, p_buffer);
    rc = glGetError();
    if (rc != GL_NO_ERROR)
    {
        LOG_ERROR("reading pixel for snapshot failed: %d\n", rc);
        free(p_buffer);
        return;
    }

    pending = (p_window->geometry.width * 3) % 4;
    image_size = (p_window->geometry.width * p_window->geometry.height * 3) +
                 (p_window->geometry.height * pending);

    p_rgb = (char*)malloc(image_size);

    for (row = 0; row < p_window->geometry.height; ++row)
    {
        for (col = 0; col < p_window->geometry.width; ++col)
        {
            offset = row * p_window->geometry.width + col;
            p_rgb[index++] = p_buffer[offset*4+2];
            p_rgb[index++] = p_buffer[offset*4+1];
            p_rgb[index++] = p_buffer[offset*4  ];
        }

        for (i = 0; i < pending; ++i)
        {
            p_rgb[index++] = 0;
        }
    }

    if (0 != g_save_dir[0])
    {
        sprintf(image_file, "%s/%s", g_save_dir, SCREENSHOT_FILE);
    }
    else
    {
        sprintf(image_file, "%s/%s", DEFAULT_SAVE_DIR, SCREENSHOT_FILE);
    }

    LOG_INFO("Output snapshot - image_file (%s)\n", image_file);
    if (0 != write_bitmap(image_file, image_size, p_rgb,
                          p_window->geometry.width, p_window->geometry.height))
    {
        LOG_ERROR("Failed to write image file\n");
    }

    free(p_buffer);
    free(p_rgb);
}

static void
RedrawHandler(struct WaylandEglWindow *p_window, void *p_data)
{
    int *p = (int*)p_data;

    static const GLfloat verts[4][3][2] = {
        {{-0.5, -0.5 }, { 0.5, -0.5 }, { 0.0,  0.5 }},
        {{ 0.5,  0.5 }, {-0.5,  0.5 }, { 0.0, -0.5 }},
        {{ 0.5,  0.0 }, {-0.5,  0.5 }, {-0.5, -0.5 }},
        {{ 0.5,  0.5 }, {-0.5,  0.0 }, { 0.5, -0.5 }}
    };
    static const GLfloat colors[3][3] = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 }
    };
    GLfloat angle = 0.0;
    GLfloat rotation[4][4] = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 },
    };
    static const int32_t speed_div = 5;
    static uint32_t start_time = 0;
    int geo = *p % 4;
    struct wl_callback *p_cb;

    DisplayAcquireWindowSurface(p_window->p_display, p_window);

    angle = ((p_window->time - start_time) / speed_div) % 360 * M_PI / 180.0;
    rotation[0][0] =  cos(angle);
    rotation[0][2] =  sin(angle);
    rotation[2][0] = -sin(angle);
    rotation[2][2] =  cos(angle);

    glViewport(0, 0, p_window->geometry.width, p_window->geometry.height);

    glUniformMatrix4fv(p_window->gl.rotation_uniform, 1, GL_FALSE,
        (GLfloat*)rotation);

    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    glVertexAttribPointer(p_window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts[geo]);
    glVertexAttribPointer(p_window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(p_window->gl.pos);
    glEnableVertexAttribArray(p_window->gl.col);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(p_window->gl.pos);
    glDisableVertexAttribArray(p_window->gl.col);

    if (1 == g_snapshot)
    {
        if (10 == ++g_frame)
        {
            glFinish();
            save_screenshot_of_window(p_window);
            g_snapshot = 0;
        }
    }

    p_cb = wl_surface_frame(p_window->p_surface);
    wl_callback_add_listener(p_cb, &frame_listener, p_window);
}

int
CreateTriangles(struct WaylandDisplay *p_display, int width, int height)
{
    struct WaylandEglWindow *p_window;
    int number = 0;

    InitGL(&p_display->surface_list);

    wl_list_for_each(p_window, &p_display->surface_list, link)
    {
        p_window->redraw_handler = RedrawHandler;
        p_window->p_user_data = malloc(sizeof(int));
        *((int*)p_window->p_user_data) = number++;

        WindowScheduleResize(p_window, width, height);
    }

    return 0;
}

static void
signal_int(int signum)
{
    _UNUSED_(signum);

    DisplayExit(gp_display);
}

static void
usage(int error_code)
{
    fprintf(stderr, "Usage: triangles [OPTIONS]\n"
        "  -t [NUMBER], --triangles=[NUMBER]  Number of displaying triangle (default: 2)\n"
        "  -n         , --no-rotation         Triangle(s) does not rotate\n"
        "  -s [PATH]  , --snapshot=[PATH]     Take snapshot at the 10th frame.\n"
        "                                     (Snapshot is saved at [PATH])\n"
        "  -w [WIDTH] , --width=[WIDTH]       Window width\n"
        "  -h [HEIGHT], --height=[HEIGHT]     Window height\n\n");
    exit(error_code);
}

#ifdef USE_TRIANGLE_MAIN
int
main(int argc, char **argv)
{
    struct WaylandEglWindow *p_window;
    char window_title[16];
    int n, i, c, no_rotation, take_snapshot;
    static const struct option longopts[] = {
        {"triangles",   required_argument, NULL, 't'},
        {"no-rotation", no_argument,       NULL, 'n'},
        {"snapshot",    required_argument, NULL, 's'},
        {"width",       required_argument, NULL, 'w'},
        {"height",      required_argument, NULL, 'h'},
        {0,          0,                    NULL, 0  }
    };
    static const char opts[] = "t:ns:w:h:";
    struct sigaction sigint;
    int window_width  = WINDOW_WIDTH;
    int window_height = WINDOW_HEIGHT;

    /* Defaults */
    n             = 2;
    no_rotation   = 0; /* false */
    take_snapshot = 0; /* false */

    if (argc == 1)
    {
        usage(EXIT_SUCCESS);
    }

    /* Process arguments */
    while ((c = getopt_long(argc, argv, opts, longopts, &i)) != -1)
    {
        switch (c) {
        case 't':
            n = optarg ? atoi(optarg) : 2;
            break;
        case 'n':
            no_rotation = 1;
            break;
        case 's':
            take_snapshot = 1;
            if (optarg)
            {
                strncpy(g_save_dir, optarg, strlen(optarg));
            }
            break;
        case 'w':
            window_width = optarg ? atoi(optarg) : WINDOW_WIDTH;
            break;
        case 'h':
            window_height = optarg ? atoi(optarg) : WINDOW_HEIGHT;
            break;
        default:
            usage(EXIT_FAILURE);
            break;
        }
    }
    g_rotation = !no_rotation;
    g_snapshot = take_snapshot;

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    gp_display = CreateDisplay(argc, argv);
    if (NULL == gp_display)
    {
        return -1;
    }

    /* Crete window */
    for (i = 0; i < n; ++i)
    {
        sprintf(window_title, "%s_%d", "window", i + 1);

        p_window = CreateEglWindow(gp_display, window_title);
        if (NULL == p_window)
        {
            LOG_ERROR("[WAR] failed to create EGL window. continue\n");
            continue;
        }
    }

    if (0 != wl_list_empty(&gp_display->surface_list))
    {
        DestroyDisplay(gp_display);
        return -1;
    }

    if (0 > CreateTriangles(gp_display, window_width, window_height))
    {
        DestroyDisplay(gp_display);
        return -1;
    }

    DisplayRun(gp_display);

    return 0;
}
#endif
