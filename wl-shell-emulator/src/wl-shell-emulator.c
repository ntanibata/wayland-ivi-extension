/*
 * Copyright (C) 2015 Advanced Driver Information Technology Joint Venture GmbH
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <weston/compositor.h>

#include "ivi-layout-export.h"
#include "ivi-controller-interface.h"

#ifndef container_of
#define container_of(ptr, type, member) ({                              \
        const __typeof__( ((type *)0)->member ) *__mptr = (ptr);        \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define DEFAULT_SURFACE_ID_FOR_WL_SHELL_APP 0x80000000

struct emulator_surface {
    struct wl_resource* resource;
    struct emulator_context *context;
    struct ivi_layout_surface *layout_surface;

    struct weston_surface *surface;
    struct wl_listener surface_destroy_listener;

    uint32_t id_surface;
    int32_t last_width;
    int32_t last_height;

    struct wl_list link;
};

struct emulator_controller {
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_client *client;
    uint32_t id;
    struct emulator_context *emulator_context;
};

struct emulator_context {
    struct wl_list controller_list;
    struct wl_list surface_list;
    struct weston_compositor *compositor;
    const struct ivi_controller_interface *ivi_controller_interface;

    uint32_t id_surface_base;
};

static void
emulator_surface_pong(struct wl_client *client,
           struct wl_resource *resource, uint32_t serial)
{

}

static void
emulator_surface_move(struct wl_client *client, struct wl_resource *resource,
           struct wl_resource *seat_resource, uint32_t serial)
{
/*Use ivi_controller_interface to implement*/
}

static void
emulator_surface_resize(struct wl_client *client, struct wl_resource *resource,
           struct wl_resource *seat_resource, uint32_t serial,
           uint32_t edges)
{
/*Use ivi_controller_interface to implement*/
}

static void
emulator_surface_set_toplevel(struct wl_client *client,
           struct wl_resource *resource)
{
/*Use ivi_controller_interface to implement*/
}

static void
emulator_surface_set_transient(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *parent_resource,
        int x, int y, uint32_t flags)
{
/*Use ivi_controller_interface to implement*/
}

static void
emulator_surface_set_fullscreen(struct wl_client *client,
           struct wl_resource *resource,
           uint32_t method,
           uint32_t framerate,
           struct wl_resource *output_resource)
{
/*Use ivi_controller_interface to implement*/
}

static void
emulator_surface_set_popup(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *seat_resource,
        uint32_t serial,
        struct wl_resource *parent_resource,
        int32_t x, int32_t y, uint32_t flags)
{

}

static void
emulator_surface_set_maximized(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *output_resource)
{

}

static void
emulator_surface_set_title(struct wl_client *client,
        struct wl_resource *resource, const char *title)
{

}

static void
emulator_surface_set_class(struct wl_client *client,
        struct wl_resource *resource, const char *class)
{

}

static const struct wl_shell_surface_interface shell_surface_implementation = {
        emulator_surface_pong,
        emulator_surface_move,
        emulator_surface_resize,
        emulator_surface_set_toplevel,
        emulator_surface_set_transient,
        emulator_surface_set_fullscreen,
        emulator_surface_set_popup,
        emulator_surface_set_maximized,
        emulator_surface_set_title,
        emulator_surface_set_class
};

/* Gets called through the weston_surface destroy signal. */
static void
emulator_handle_surface_destroy(struct wl_listener *listener, void *data)
{
    struct emulator_surface *emulator_surface =
            container_of(listener, struct emulator_surface,
                surface_destroy_listener);

    assert(emulator_surface != NULL);

    if (emulator_surface->surface!=NULL) {
        emulator_surface->surface->configure = NULL;
        emulator_surface->surface->configure_private = NULL;
        emulator_surface->surface = NULL;
    }

    wl_list_remove(&emulator_surface->surface_destroy_listener.link);
    wl_list_remove(&emulator_surface->link);

    if (emulator_surface->resource != NULL) {
        wl_resource_set_user_data(emulator_surface->resource, NULL);
        emulator_surface->resource = NULL;
    }
    free(emulator_surface);
}

static void
emulator_surface_configure(struct weston_surface *surface,
                           int32_t sx, int32_t sy)
{
    struct emulator_surface *surf = surface->configure_private;
    struct emulator_context *ctx;

    if ((surf == NULL) ||
        (surf->layout_surface == NULL)) {
        return;
    }

    if ((surf->last_width != surface->width) ||
        (surf->last_height != surface->height)) {

        surf->last_width = surface->width;
        surf->last_height = surface->height;

        ctx = surf->context;
        ctx->ivi_controller_interface->surface_configure(surf->layout_surface,
                                                         surface->width,
                                                         surface->height);
    }
}

/*
 * The ivi_surface wl_resource destructor.
 *
 * Gets called via ivi_surface.destroy request or automatic wl_client clean-up.
 */
static void
shell_destroy_shell_surface(struct wl_resource *resource)
{
    struct emulator_surface *surf = wl_resource_get_user_data(resource);
    if (surf != NULL) {
        surf->resource = NULL;
    }
}

static void
emulator_get_shell_surface(struct wl_client *client, struct wl_resource *resource,
                           uint32_t id, struct wl_resource *surface_resource)
{
    struct emulator_controller *controller = wl_resource_get_user_data(resource);
    struct emulator_context * context = controller->emulator_context;
    struct emulator_surface *emulator_surface;
    struct weston_surface *surface;
    struct ivi_layout_surface *layout_surface;
    struct wl_resource *res;
    const struct ivi_controller_interface *interface =
                            context->ivi_controller_interface;

    surface = wl_resource_get_user_data(surface_resource);

    layout_surface = interface->surface_create(surface, context->id_surface_base);

    if (!layout_surface) {
        wl_resource_post_no_memory(surface_resource);
        return;
    }

    emulator_surface = zalloc(sizeof *emulator_surface);
    if (emulator_surface == NULL) {
        wl_resource_post_no_memory(resource);
        return;
    }

    emulator_surface->layout_surface = layout_surface;
    emulator_surface->id_surface = context->id_surface_base;
    emulator_surface->last_width = 0;
    emulator_surface->last_height = 0;

    wl_list_init(&emulator_surface->link);
    wl_list_insert(&context->surface_list, &emulator_surface->link);

    emulator_surface->context = context;
    ++(context->id_surface_base);

    /** The following code relies on wl_surface destruction triggering
    * immediateweston_surface destruction
    */
    emulator_surface->surface_destroy_listener.notify = emulator_handle_surface_destroy;
    wl_signal_add(&surface->destroy_signal,
                  &emulator_surface->surface_destroy_listener);

    emulator_surface->surface = surface;
    surface->configure = emulator_surface_configure;
    surface->configure_private = emulator_surface;

    res = wl_resource_create(client, &wl_shell_surface_interface, 1, id);
    if (res == NULL) {
        wl_client_post_no_memory(client);
        return;
    }

    emulator_surface->resource = res;

    wl_resource_set_implementation(res, &shell_surface_implementation,
                                  emulator_surface, shell_destroy_shell_surface);
}
static const struct wl_shell_interface shell_implementation = {
    emulator_get_shell_surface
};

static void
unbind_resource_controller(struct wl_resource *resource)
{
    struct emulator_controller *controller = wl_resource_get_user_data(resource);

    wl_list_remove(&controller->link);

    free(controller);
}

static void
bind_shell_implementation(struct wl_client *client, void *data,
               uint32_t version, uint32_t id)
{
    struct emulator_context *ctx = data;
    struct emulator_controller *controller;
    controller = calloc(1, sizeof *controller);
    if (controller == NULL) {
        weston_log("%s: Failed to allocate memory for controller\n",
                   __FUNCTION__);
        return;
    }

    controller->emulator_context = ctx;
    controller->resource =
        wl_resource_create(client, &wl_shell_interface, 1, id);
    wl_resource_set_implementation(controller->resource, &shell_implementation,
                                   controller, unbind_resource_controller);

    controller->client = client;
    controller->id = id;

    wl_list_insert(&ctx->controller_list, &controller->link);
}

static struct emulator_context *
create_emulator_context(struct weston_compositor *ec,
                     const struct ivi_controller_interface *interface)
{
    struct emulator_context *ctx = NULL;
    ctx = calloc(1, sizeof *ctx);
    if (ctx == NULL) {
        weston_log("%s: Failed to allocate memory for input context\n",
                   __FUNCTION__);
        return NULL;
    }

    memset(ctx, 0, sizeof *ctx);

    ctx->compositor = ec;
    ctx->ivi_controller_interface = interface;
    wl_list_init(&ctx->controller_list);
    wl_list_init(&ctx->surface_list);

    return ctx;
}

static int
setup_emulator_config(struct emulator_context *ctx, int *argc, char *argv[])
{
    struct weston_config_section *section;

    section = weston_config_get_section(ctx->compositor->config, "ivi-shell",
                                        NULL, NULL);

    if (section) {
        weston_config_section_get_uint(section, "surface-id-for-wl-shell-app",
                                       &ctx->id_surface_base,
                                       DEFAULT_SURFACE_ID_FOR_WL_SHELL_APP);

        weston_log("Based surface ID for wl_shell application: %u (0x%x)\n",
                   ctx->id_surface_base, ctx->id_surface_base);
    }

    return 0;
}

WL_EXPORT int
controller_module_init(struct weston_compositor *ec, int* argc, char *argv[],
                       const struct ivi_controller_interface *interface,
                       size_t interface_version)
{
    struct emulator_context *ctx = create_emulator_context(ec, interface);
    if (ctx == NULL) {
        weston_log("%s: Failed to create input context\n", __FUNCTION__);
        return -1;
    }

    if (setup_emulator_config(ctx, argc, argv)) {
        return -1;
    }

    if (wl_global_create(ec->wl_display, &wl_shell_interface, 1,
                         ctx, bind_shell_implementation) == NULL) {
        return -1;
    }
    weston_log("wl-shell-emulator module loaded successfully!\n");
    return 0;
}
