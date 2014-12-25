/*
 * Copyright Ac 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>


#include "ivi-application-client-protocol.h"

#define WINDOW_TITLE "window_1"

#define UNUSED(x) (void)x

#ifdef UNUSED
#elif defined(__GNUC__)
#    define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
#    define UNUSED(x) /*@unused@*/ x
#else
#    define UNUSED(x) x
#endif

#define USE_WL_SEAT 1

struct window;

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *wl_shell;
#if USE_WL_SEAT
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_touch *touch;
    struct wl_shm *shm;
#endif
    struct {
        EGLDisplay dpy;
        EGLContext ctx;
#ifdef CREATE_SHARED_CONTEXT
        EGLContext ctx_shared;
#endif
        EGLConfig conf;
    } egl;
    struct window *window;
    struct ivi_application *ivi_application;
};

struct geometry {
    int width, height;
};

struct window {
    struct display *display;
    struct geometry geometry, window_size;
    struct {
        GLuint rotation_uniform;
        GLuint pos;
        GLuint col;
    } gl;

    struct wl_egl_window *native;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct ivi_surface *ivi_surface;
    uint32_t id_ivisurf;
    int ivi_surface_state; /* if not zero, ivi_surface is not available */
    EGLSurface egl_surface;
    struct wl_callback *callback;
    int fullscreen, configured, opaque;
};

static const char *vert_shader_text =
    "uniform mat4 rotation;\n"
    "attribute vec4 pos;\n"
    "attribute vec4 color;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = rotation * pos;\n"
    "  v_color = color;\n"
    "}\n";

static const char *frag_shader_text =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

static int running = 1;

static void
init_egl(struct display *display, int opaque)
{
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint imajor, iminor, n;
    EGLBoolean ret;

    if (opaque)
        config_attribs[9] = 0;

    display->egl.dpy = eglGetDisplay(display->display);
    assert(display->egl.dpy);

    ret = eglInitialize(display->egl.dpy, &imajor, &iminor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    ret = eglChooseConfig(display->egl.dpy, config_attribs,
                  &display->egl.conf, 1, &n);
    assert(ret && n == 1);

    display->egl.ctx = eglCreateContext(display->egl.dpy,
                        display->egl.conf,
                        EGL_NO_CONTEXT, context_attribs);
    assert(display->egl.ctx);

#ifdef CREATE_SHARED_CONTEXT
    display->egl.ctx_shared = eglCreateContext(display->egl.dpy,
                                               display->egl.conf,
                                               display->egl.ctx,
                                               context_attribs);
    EGLint err = eglGetError();
    if (err == EGL_SUCCESS) {
        printf("The 2nd EGLContext creation: SUCCESS: Context Address: 0x%08x\n", display->egl.ctx_shared);
    } else {
        printf("The 2nd EGLContext creation: FAILED (0x%x)\n", err);
    }
#endif
}

static void
fini_egl(struct display *display)
{
    eglTerminate(display->egl.dpy);
    eglReleaseThread();
}

static GLuint
create_shader(struct window *window, const char *source, GLenum shader_type)
{
    UNUSED(window);

    GLuint shader;
    GLint status;

    shader = glCreateShader(shader_type);
    assert(shader != 0);

    glShaderSource(shader, 1, (const char **) &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char msg[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, msg);
        fprintf(stderr, "Error: compiling %s: %*s\n",
            shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
            len, msg);
        exit(1);
    }

    return shader;
}

static void
init_gl(struct window *window)
{
    GLuint frag, vert;
    GLuint program;
    GLint status;

    frag = create_shader(window, frag_shader_text, GL_FRAGMENT_SHADER);
    vert = create_shader(window, vert_shader_text, GL_VERTEX_SHADER);

    program = glCreateProgram();
    glAttachShader(program, frag);
    glAttachShader(program, vert);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char msg[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, msg);
        fprintf(stderr, "Error: linking:\n%*s\n", len, msg);
        exit(1);
    }

    glUseProgram(program);

    window->gl.pos = 0;
    window->gl.col = 1;

    glBindAttribLocation(program, window->gl.pos, "pos");
    glBindAttribLocation(program, window->gl.col, "color");
    glLinkProgram(program);

    window->gl.rotation_uniform =
        glGetUniformLocation(program, "rotation");
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
        uint32_t serial)
{
    UNUSED(data);

    wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
         uint32_t edges, int32_t width, int32_t height)
{
    UNUSED(shell_surface);
    UNUSED(edges);

    struct window *window = data;

    if (window->native)
        wl_egl_window_resize(window->native, width, height, 0, 0);

    window->geometry.width = width;
    window->geometry.height = height;

    if (!window->fullscreen)
        window->window_size = window->geometry;
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
    UNUSED(data);
    UNUSED(shell_surface);
}

static struct wl_shell_surface_listener wl_shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};

static void
redraw(void *data, struct wl_callback *callback, uint32_t time);

static void
configure_callback(void *data, struct wl_callback *callback, uint32_t  time)
{
    struct window *window = data;

    wl_callback_destroy(callback);

    window->configured = 1;

    if (window->callback == NULL)
        redraw(data, NULL, time);
}

static struct wl_callback_listener configure_callback_listener = {
    configure_callback,
};

static void
toggle_fullscreen(struct window *window, int fullscreen)
{
    struct display *display = window->display;

    window->fullscreen = fullscreen;
    window->configured = 0;

    if (fullscreen) {
        if (display->wl_shell) {
            wl_shell_surface_set_fullscreen(window->shell_surface,
                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                    0, NULL);
        }
    } else {
        if (display->wl_shell) {
            wl_shell_surface_set_toplevel(window->shell_surface);
        }
        handle_configure(window, window->shell_surface, 0,
                 window->window_size.width,
                 window->window_size.height);
    }

    struct wl_callback *callback = wl_display_sync(window->display->display);
    wl_callback_add_listener(callback, &configure_callback_listener,
                 window);
}

static void
ivi_surface_visibility(void *data, struct ivi_surface *ivi_surface,
               int32_t visibility)
{
    UNUSED(data);
    UNUSED(ivi_surface);
    UNUSED(visibility);
}

static void
ivi_surface_warning(void *data, struct ivi_surface *ivi_surface,
            int32_t warning_code, const char *warning_text)
{
    UNUSED(ivi_surface);

    struct window *window = data;

    fprintf(stderr, "receive ivi_surface_warning event:\n"
            "    warning code: %d\n"
            " warning message: %s\n",
            warning_code, warning_text);

    window->ivi_surface_state = -1;
}

static const struct ivi_surface_listener ivi_surface_event_listener = {
    ivi_surface_visibility,
    ivi_surface_warning
};

static void
create_surface(struct window *window)
{
    struct display *display = window->display;
    EGLBoolean ret;
    EGLint min_swap_interval, max_swap_interval;

    window->surface = wl_compositor_create_surface(display->compositor);
    if (display->wl_shell) {
        window->shell_surface = wl_shell_get_shell_surface(display->wl_shell,
                window->surface);
        wl_shell_surface_add_listener(window->shell_surface,
                &wl_shell_surface_listener, window);
    }

    window->native =
        wl_egl_window_create(window->surface,
                     window->window_size.width,
                     window->window_size.height);

    window->egl_surface =
        eglCreateWindowSurface(display->egl.dpy,
                       display->egl.conf,
                       window->native, NULL);

    if (display->wl_shell) {
        wl_shell_surface_set_title(window->shell_surface, WINDOW_TITLE);
    }

    ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
                 window->egl_surface, window->display->egl.ctx);
    assert(ret == EGL_TRUE);

    /* setup swap interval */
    eglGetConfigAttrib(window->display->egl.dpy, window->display->egl.conf,
                       EGL_MIN_SWAP_INTERVAL, &min_swap_interval);
    eglGetConfigAttrib(window->display->egl.dpy, window->display->egl.conf,
                       EGL_MAX_SWAP_INTERVAL, &max_swap_interval);
    printf("EGL min swap interval: %d\n", min_swap_interval);
    printf("EGL max swap interval: %d\n", max_swap_interval);

    eglSwapInterval(window->display->egl.dpy, min_swap_interval);

    toggle_fullscreen(window, window->fullscreen);

    if (display->ivi_application) {
        wl_display_roundtrip(display->display);

        window->ivi_surface =
            ivi_application_surface_create(display->ivi_application,
                               window->id_ivisurf,
                               window->surface);
        ivi_surface_add_listener(window->ivi_surface,
                     &ivi_surface_event_listener, window);
    }
}

static void
destroy_surface(struct window *window)
{
    struct display *display = window->display;

    /* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
     * on eglReleaseThread(). */
    eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
               EGL_NO_CONTEXT);

    eglDestroySurface(window->display->egl.dpy, window->egl_surface);
    wl_egl_window_destroy(window->native);

    if (display->wl_shell) {
        wl_shell_surface_destroy(window->shell_surface);
    }

    wl_surface_destroy(window->surface);

    if (window->callback)
        wl_callback_destroy(window->callback);
}

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
    redraw
};

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    struct window *window = data;
    static const GLfloat verts[3][2] = {
        { -0.5, -0.5 },
        {  0.5, -0.5 },
        {  0,    0.5 }
    };
    static const GLfloat colors[3][3] = {
        { 1, 0, 1 },
        { 1, 1, 0 },
        { 0, 1, 1 }
    };
    GLfloat angle;
    GLfloat rotation[4][4] = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
    static const int32_t speed_div = 5;
    static uint32_t start_time = 0;
    struct wl_region *region;

    assert(window->callback == callback);
    window->callback = NULL;

    if (callback)
        wl_callback_destroy(callback);

    if (!window->configured)
        return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    if (start_time == 0)
        start_time = time;

    angle = (float)(((uint32_t)((time - start_time) / speed_div)) % 360) * M_PI / 180.0;
    rotation[0][0] =  cos(angle);
    rotation[0][2] =  sin(angle);
    rotation[2][0] = -sin(angle);
    rotation[2][2] =  cos(angle);

    glViewport(0, 0, window->geometry.width, window->geometry.height);

    glUniformMatrix4fv(window->gl.rotation_uniform, 1, GL_FALSE,
               (GLfloat *) rotation);

    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(window->gl.pos);
    glEnableVertexAttribArray(window->gl.col);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(window->gl.pos);
    glDisableVertexAttribArray(window->gl.col);

    if (window->opaque || window->fullscreen) {
        region = wl_compositor_create_region(window->display->compositor);
        wl_region_add(region, 0, 0,
                  window->geometry.width,
                  window->geometry.height);
        wl_surface_set_opaque_region(window->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_opaque_region(window->surface, NULL);
    }

    eglSwapBuffers(window->display->egl.dpy, window->egl_surface);
}

#if USE_WL_SEAT

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
             uint32_t serial, struct wl_surface *surface,
             wl_fixed_t sx, wl_fixed_t sy)
{
    UNUSED(surface);
    UNUSED(sx);
    UNUSED(sy);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
             uint32_t serial, struct wl_surface *surface)
{
    UNUSED(data);
    UNUSED(pointer);
    UNUSED(serial);
    UNUSED(surface);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
              uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    UNUSED(data);
    UNUSED(pointer);
    UNUSED(time);
    UNUSED(sx);
    UNUSED(sy);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
              uint32_t serial, uint32_t time, uint32_t button,
              uint32_t state)
{
    UNUSED(wl_pointer);
    UNUSED(time);

    struct display *display = data;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (display->wl_shell) {
            wl_shell_surface_move(display->window->shell_surface,
                      display->seat, serial);
        }
    }
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
            uint32_t time, uint32_t axis, wl_fixed_t value)
{
    UNUSED(data);
    UNUSED(wl_pointer);
    UNUSED(time);
    UNUSED(axis);
    UNUSED(value);
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
               uint32_t format, int fd, uint32_t size)
{
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(format);
    UNUSED(fd);
    UNUSED(size);
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, struct wl_surface *surface,
              struct wl_array *keys)
{
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(surface);
    UNUSED(keys);
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, struct wl_surface *surface)
{
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(surface);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
            uint32_t serial, uint32_t time, uint32_t key,
            uint32_t state)
{
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(time);

    struct display *d = data;

    if (key == KEY_F11 && state)
        toggle_fullscreen(d->window, d->window->fullscreen ^ 1);
    else if (key == KEY_ESC && state)
        running = 0;
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, uint32_t mods_depressed,
              uint32_t mods_latched, uint32_t mods_locked,
              uint32_t group)
{
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(mods_depressed);
    UNUSED(mods_latched);
    UNUSED(mods_locked);
    UNUSED(group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
                  uint32_t serial, uint32_t time, struct wl_surface *surface,
                  int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    printf("[%s(%d)] window.c pid=%u, id=%d, x=%d, y=%d\n", __func__, __LINE__, getpid(), x, y, id);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
                uint32_t serial, uint32_t time, int32_t id)
{
    printf("[%s(%d)] window.c pid=%u id=%d\n", __func__, __LINE__, getpid(), id);
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
                    uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    printf("[%s(%d)] window.c pid=%u, id=%d, x=%d, y=%d\n", __func__, __LINE__, getpid(), id, x, y);
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
    printf("[%s(%d)] window.c pid=%u\n", __func__, __LINE__, getpid());
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
    printf("[%s(%d)] window.c pid=%u\n", __func__, __LINE__, getpid());
}

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
             uint32_t caps)
{
    struct display *d = data;

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
        d->touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(d->touch, d);
        wl_touch_add_listener(d->touch, &touch_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
        wl_touch_destroy(d->touch);
        d->touch = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

#endif /* USE_WL_SEAT */

static void
registry_handle_global(void *data, struct wl_registry *registry,
               uint32_t name, const char *interface,
                       uint32_t version)
{
    UNUSED(version);

    struct display *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor =
            wl_registry_bind(registry, name,
                     &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->wl_shell = wl_registry_bind(registry, name,
                           &wl_shell_interface, 1);
#if USE_WL_SEAT
    /* Commented: This sample doesn't receive input events any longer */
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = wl_registry_bind(registry, name,
                       &wl_seat_interface, 1);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = wl_registry_bind(registry, name,
                      &wl_shm_interface, 1);
#endif
    } else if (strcmp(interface, "ivi_application") == 0) {
        d->ivi_application =
            wl_registry_bind(registry, name,
                     &ivi_application_interface, 1);
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    NULL
};

static void
signal_int(int signum)
{
    UNUSED(signum);

    running = 0;
}

static struct wl_display*
connect_display(const char *p_name)
{
    static const int max_retry_count = 500;
    struct wl_display *p_display = NULL;
    int retry_count = 0;

    do
    {
        if (NULL != (p_display = wl_display_connect(p_name)))
        {
            break;
        }
        usleep(10000);
    } while (++retry_count < max_retry_count);

    if (p_display)
    {
        printf("Connected to wl_display. (Retried %d times)\n", retry_count);
    }
    else
    {
        printf("Failed to connect wl_display. given up.\n");
    }

    return p_display;
}

static void
usage(int error_code)
{
    fprintf(stderr, "Usage: simple-egl-wl-shell [OPTIONS]\n\n"
        "  --surface(-s)  Surface ID for ivi_application (default: 1500)\n"
        "  --help(-h)     This help text\n\n");

    exit(error_code);
}

int
main(int argc, char **argv)
{
    struct sigaction sigint;
    struct display display;
    struct window  window;
    int i, c, ret = 0;
    static const struct option longopts[] = {
        {"surfce", optional_argument, NULL, 's'},
        {"help",   optional_argument, NULL, 'h'},
        {0,        0,                 NULL, 0  }
    };
    static const char opts[] = "s:h";
    uint32_t id_surface = 1500;

    while ((c = getopt_long(argc, argv, opts, longopts, &i)) != -1){
        switch (c) {
        case 's':
            id_surface = optarg ? atoi(optarg) : 1500;
            break;
        case 'h':
            break;
        default:
            break;
        }
    }

    memset(&display, 0x00, sizeof(struct display));
    memset(&window, 0x00, sizeof(struct window));

    window.display = &display;
    display.window = &window;
    window.window_size.width  = 250;
    window.window_size.height = 250;
    window.id_ivisurf = id_surface;

    display.display = connect_display(NULL);
    assert(display.display);

    display.registry = wl_display_get_registry(display.display);
    wl_registry_add_listener(display.registry,
                 &registry_listener, &display);

    wl_display_dispatch(display.display);
    wl_display_roundtrip(display.display);

    init_egl(&display, window.opaque);
    create_surface(&window);
    init_gl(&window);

    wl_display_dispatch(display.display);
    wl_display_roundtrip(display.display);

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = (int)SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    printf("*********************************************************::\n");

    while (running && ret != -1) {
        wl_display_dispatch_pending(display.display);
        redraw(&window, NULL, 0);
    }

    fprintf(stderr, "simple-egl exiting\n");

    if (window.ivi_surface)
        ivi_surface_destroy(window.ivi_surface);
    if (window.display->ivi_application)
        ivi_application_destroy(window.display->ivi_application);

    destroy_surface(&window);
    fini_egl(&display);

    if (display.wl_shell) {
        wl_shell_destroy(display.wl_shell);
    }

    if (display.compositor)
        wl_compositor_destroy(display.compositor);

    wl_display_flush(display.display);
    wl_display_disconnect(display.display);

    return 0;
}
