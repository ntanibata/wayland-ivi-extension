#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <wayland-client-protocol.h>

#include "ivi-application-client-protocol.h"
#include "ivi-controller-client-protocol.h"

#define ASSERT(x) assert(x)

struct display {
    struct wl_display      *display;
    struct wl_registry     *registry;
    struct ivi_application *ivi_application;
    struct ivi_controller  *ivi_controller;
    uint32_t                surface_id;
    int32_t                 error_code;
    int                     surface_created;
};

static int running = 1;

static void
controller_event_screen(void *data, struct ivi_controller *ivi_controller,
                        uint32_t id_screen, struct ivi_controller_screen *screen)
{
    /* do nothing */
}

static void
controller_event_layer(void *data, struct ivi_controller *ivi_controller,
                       uint32_t id_layer)
{
    /* do nothing */
}

static void
controller_event_surface(void *data, struct ivi_controller *ivi_controller,
                         uint32_t id_surface)
{
    /* do nothing */
}

static void
controller_event_error(void *data, struct ivi_controller *ivi_controller,
                       int32_t object_id, int32_t object_type, int32_t error_code,
                       const char *error_text)
{
    struct display *d = data;

    printf("IVISurfaceCreator: receive event [error=%d]\n", error_code);

    if (object_type == IVI_CONTROLLER_OBJECT_TYPE_SURFACE)
    {
        d->error_code = error_code;
    }
}

static void
controller_event_native_handle(void *data, struct ivi_controller *ivi_controller,
                               struct wl_surface *surface)
{
    struct display *d = data;

    printf("IVISurfaceCreator: receive event [native_handle]\n");

    if (d && d->ivi_application)
    {
        printf("IVISurfaceCreator: create ivi_surface (ID:%u)\n", d->surface_id);
        ivi_application_surface_create(d->ivi_application,
                                       d->surface_id, surface);
        ++(d->surface_id);
        d->surface_created = 1;
    }
}

static const struct ivi_controller_listener controller_listener = {
    controller_event_screen,
    controller_event_layer,
    controller_event_surface,
    controller_event_error,
    controller_event_native_handle
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
                       uint32_t name, const char *interface, uint32_t version)
{
    struct display *d = data;

    if (strcmp(interface, "ivi_application") == 0)
    {
        d->ivi_application = wl_registry_bind(registry, name,
                                              &ivi_application_interface, 1);
    }
    else if (strcmp(interface, "ivi_controller") == 0)
    {
        d->ivi_controller = wl_registry_bind(registry, name,
                                             &ivi_controller_interface, 1);
        ivi_controller_add_listener(d->ivi_controller,
                                    &controller_listener, data);
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{
    /* do nothing */
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
signal_action(int signum)
{
    running = 0;
}

static void
usage(int status)
{
    fprintf(stderr, "Usage: IVISurfaceCreator <Process ID> <Window Title> <IVI-Surface ID>\n");
    exit(status);
}

int
main(int argc, char **argv)
{
    struct display display = {0};
    struct sigaction sigact;

    if (argc < 4)
    {
        usage(EXIT_FAILURE);
    }

    int32_t process_id   = atoi(argv[1]);
    char *  window_title = NULL;
    display.surface_id   = atoi(argv[3]);

    if (strlen(argv[2]) > 0)
    {
        window_title = strdup(argv[2]);
    }

    sigact.sa_handler = signal_action;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigact, NULL);

    display.display = wl_display_connect(NULL);
    ASSERT(display.display);

    display.registry = wl_display_get_registry(display.display);
    wl_registry_add_listener(display.registry,
                             &registry_listener, &display);

    wl_display_dispatch(display.display);

    ASSERT(display.ivi_application &&
           display.ivi_controller);
    int retry_count = 0;
    int rc = 0;
    do
    {
        ivi_controller_get_native_handle(display.ivi_controller,
                                         process_id,
                                         window_title);

        wl_display_roundtrip(display.display);

        while (!display.error_code && running && (rc != -1))
        {
            rc = wl_display_dispatch(display.display);
        }

        if (display.error_code == IVI_CONTROLLER_ERROR_CODE_NATIVE_HANDLE_END)
        {
            if (display.surface_created){
                break;
            } else
            {
                printf("IVISurfaceCreator: Search of native handle was ended.\n"
                       "                   But no surface was created.\n"
                       "                   Retry get_native_handle.\n");
                display.error_code = 0;
                sleep(1);
            }
        }

    } while ((++retry_count < 10) && running);

    wl_display_roundtrip(display.display);

    printf("IVISurfaceCreator: exit\n");

    wl_registry_destroy(display.registry);
    wl_display_flush(display.display);
    wl_display_disconnect(display.display);

    if (window_title)
        free(window_title);

    return 0;
}
