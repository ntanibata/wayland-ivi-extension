#ifdef USE_DRM
#include <stdio.h>
#include <stdint.h>

#include <fcntl.h>
#include <drm.h>
#include <gbm.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "wayland-server.h"
#include "weston/compositor.h"
#include "ivi-share-extension-server-protocol.h"
#include "ivi-layout-export.h"
#include "texture-sharing.h"

/**
 * convenience macro to access single bits of a bitmask
 */
#define IVI_BIT(x) (1 << (x))
#define container_of(ptr, type, member) ({				\
	const __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct ivi_nativesurface
{
    struct weston_surface *surface; /* resource                                   */
    uint32_t id;                    /* object id                                  */
    struct wl_list link;            /* link                                       */
    struct wl_list client_list;     /* ivi_nativesurface_client_link list         */
    uint32_t bufferType;            /* buffer type (GBM only)                     */
    uint32_t name;                  /* buffer name                                */
    uint32_t width;                 /* buffer width                               */
    uint32_t height;                /* buffer height                              */
    uint32_t stride;                /* buffer stride[LSB:byte]                    */
    uint32_t format;                /* ARGB8888                                   */
    char *title;
    uint32_t pid;
    struct wl_listener surface_destroy_listener;
};

struct ivi_nativesurface_client_link
{
    struct wl_resource *resource;
    struct wl_client *client;
    bool firstSendConfigureComp;
    struct wl_list link;                         /* ivi_nativesurface link */
    struct ivi_nativesurface *parent;
    bool configure_sent;
};

struct shell_surface
{
    struct wl_resource *resource;
    struct wl_listener surface_destroy_listener;
    struct weston_surface *surface;
    pid_t pid;
    char *title;
    struct wl_list link;                         /* ivi_shell_ext link */
};

struct ivi_shell_ext
{
    struct weston_compositor *wc;
    struct wl_resource *resource;
    struct wl_listener destroy_listener;
    struct wl_list list_shell_surface;           /* shell_surface list */
    struct wl_list list_nativesurface;           /* ivi_nativesurface list */
    struct wl_list list_redirect_target;	 /* redirect_target list */
};

enum ivi_sharesurface_updatetype
{
    IVI_SHARESURFACE_STABLE = IVI_BIT(0),
    IVI_SHARESURFACE_UPDATE = IVI_BIT(1),
    IVI_SHARESURFACE_CONFIGURE = IVI_BIT(2)
};

struct udev_input{};

struct drm_compositor {
	struct weston_compositor base;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_drm_source;

	struct {
		int id;
		int fd;
		char *filename;
	} drm;
	struct gbm_device *gbm;
	uint32_t *crtcs;
	int num_crtcs;
	uint32_t crtc_allocator;
	uint32_t connector_allocator;
	struct wl_listener session_listener;
	uint32_t format;

	/* we need these parameters in order to not fail drmModeAddFB2()
	 * due to out of bounds dimensions, and then mistakenly set
	 * sprites_are_broken:
	 */
	uint32_t min_width, max_width;
	uint32_t min_height, max_height;
	int no_addfb2;

	struct wl_list sprite_list;
	int sprites_are_broken;
	int sprites_hidden;

	int cursors_are_broken;

	int use_pixman;

	uint32_t prev_state;

	clockid_t clock;
	struct udev_input input;
};

struct redirect_target {
	struct wl_client *client;
	struct wl_resource *resource;
	struct wl_resource *target_resource;
	uint32_t id;
	struct wl_list link;
};

static struct ivi_shell_ext *
get_instance(void)
{
    static struct ivi_shell_ext *shell_ext = NULL;
    if (NULL == shell_ext) {
        shell_ext = calloc(1, sizeof(*shell_ext));
        wl_list_init(&shell_ext->list_shell_surface);
        wl_list_init(&shell_ext->list_nativesurface);
	wl_list_init(&shell_ext->list_redirect_target);
    }
    return shell_ext;
}

static void
remove_shell_surface(struct wl_resource *resource)
{
    struct shell_surface *shsurf = wl_resource_get_user_data(resource);
    if (NULL == shsurf) {
        return;
    }

    wl_list_remove(&shsurf->link);

    if (NULL != shsurf->title) {
        free(shsurf->title);
        shsurf->title = NULL;
    }

    free(shsurf);
}

static void
free_nativesurface(struct ivi_nativesurface *nativesurf)
{
    if (NULL == nativesurf) {
        return;
    }

    if (NULL != nativesurf->title) {
        free(nativesurf->title);
        nativesurf->title = NULL;
    }
    nativesurf->surface_destroy_listener.notify = NULL;
    wl_list_remove(&nativesurf->surface_destroy_listener.link);
    free(nativesurf);
}

void
cleanup_texture_sharing(struct wl_client *client)
{
    if (NULL == client) {
        return;
    }

    struct ivi_nativesurface *nativesurf = NULL;
    struct ivi_nativesurface *nativesurf_next = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    wl_list_for_each_safe(nativesurf, nativesurf_next, &shell_ext->list_nativesurface, link) {
        struct ivi_nativesurface_client_link *p_link = NULL;
        struct ivi_nativesurface_client_link *p_next = NULL;
        wl_list_for_each_safe(p_link, p_next, &nativesurf->client_list, link) {
            if (p_link->client == client) {
                if (p_link->resource) {
                    wl_resource_destroy(p_link->resource);
                    p_link->resource = NULL;
                }

                if (wl_list_empty(&nativesurf->client_list))
                {
                    wl_list_remove(&nativesurf->link);
                    free_nativesurface(nativesurf);
                }
                return;
            }
        }
    }
}

static void
remove_client_link(struct ivi_nativesurface_client_link *client_link)
{
    if (NULL == client_link) {
        return;
    }

    struct ivi_shell_ext *shell_ext = get_instance();
    struct ivi_nativesurface *nativesurf = NULL;
    struct ivi_nativesurface *nativesurf_next = NULL;
    wl_list_for_each_safe(nativesurf, nativesurf_next, &shell_ext->list_nativesurface, link) {
        struct ivi_nativesurface_client_link *p_link = NULL;
        struct ivi_nativesurface_client_link *p_next = NULL;
        wl_list_for_each_safe(p_link, p_next, &nativesurf->client_list, link) {
            if (p_link == client_link) {
                if (p_link->resource) {
                    wl_resource_destroy(p_link->resource);
                }

                if (wl_list_empty(&nativesurf->client_list))
                {
                    wl_list_remove(&nativesurf->link);
                    free_nativesurface(nativesurf);
                }
                return;
            }
        }
    }
}

static void
remove_nativesurface(struct ivi_nativesurface *nativesurf)
{
    if (NULL == nativesurf) {
        return;
    }

    struct ivi_nativesurface_client_link *p_link = NULL;
    struct ivi_nativesurface_client_link *p_next = NULL;
    wl_list_for_each_safe(p_link, p_next, &nativesurf->client_list, link) {
        if (p_link->resource) {
            wl_resource_destroy(p_link->resource);
        }
    }
    wl_list_remove(&nativesurf->link);
    free_nativesurface(nativesurf);
}

static void
ivi_shell_ext_destroy(struct wl_listener *listener, void *data)
{
    struct ivi_shell_ext *shell_ext = get_instance();

    struct shell_surface *shsurf = NULL;
    struct shell_surface *p_next = NULL;
    wl_list_for_each_safe(shsurf, p_next, &shell_ext->list_shell_surface, link) {
        wl_list_remove(&shsurf->link);
        free(shsurf);
    }

    free(shell_ext);
}

static void
share_surface_destroy(struct wl_client *client,
                      struct wl_resource *resource)
{
    if (NULL == resource) {
        return;
    }

    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);
    if (NULL == client_link) {
        weston_log("Can not execute share_surface_destroy. p_resource == NULL\n");
        return;
    }

    remove_client_link(client_link);
}

static uint32_t
get_event_time()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static struct weston_seat *
get_weston_seat(struct weston_compositor *compositor, struct ivi_nativesurface_client_link *client_link)
{
    assert(compositor);
    assert(client_link);

    struct weston_seat *link = NULL;
    struct wl_client *target_client = wl_resource_get_client(client_link->parent->surface->resource);

    wl_list_for_each(link, &compositor->seat_list, link) {
        struct wl_resource *res;
        wl_list_for_each(res, &link->base_resource_list, link) {
            struct wl_client *client = wl_resource_get_client(res);
            if (target_client == client && link->touch != NULL) {
                return link;
            }
        }
    }

    return NULL;
}

static void
share_surface_redirect_touch_down(struct wl_client *client,
                                  struct wl_resource *resource,
                                  uint32_t serial,
                                  int32_t id,
                                  wl_fixed_t x,
                                  wl_fixed_t y)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_compositor *compositor = shell_ext->wc;
    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);
    struct weston_seat *seat = NULL;
    struct wl_resource *target_resource = NULL;
    uint32_t time = get_event_time();
    struct redirect_target *redirect_target = malloc(sizeof *redirect_target);
    struct wl_resource *surface_resource = client_link->parent->surface->resource;

    seat = get_weston_seat(compositor, client_link);
    if (seat == NULL) {
        return;
    }

    wl_list_for_each(target_resource, &seat->touch->resource_list, link) {
        if (wl_resource_get_client(target_resource) == wl_resource_get_client(surface_resource)) {
            uint32_t new_serial = wl_display_next_serial(compositor->wl_display);
            wl_touch_send_down(target_resource, new_serial, time, surface_resource, id, x, y);
            break;
        }
    }

    redirect_target->client = client;
    redirect_target->resource = resource;
    redirect_target->target_resource = target_resource;
    redirect_target->id = id;
    wl_list_insert(&shell_ext->list_redirect_target, &redirect_target->link);
}

static void
share_surface_redirect_touch_up(struct wl_client *client,
                                struct wl_resource *resource,
                                uint32_t serial,
                                int32_t id)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_compositor *compositor = shell_ext->wc;
    struct redirect_target *redirect_target = NULL;
    struct redirect_target *next = NULL;
    uint32_t new_serial = wl_display_next_serial(compositor->wl_display);
    uint32_t time = get_event_time();

    wl_list_for_each_safe(redirect_target, next, &shell_ext->list_redirect_target, link) {
        if (client == redirect_target->client && resource == redirect_target->resource && id == redirect_target->id) {
            wl_touch_send_up(redirect_target->target_resource, new_serial, time, id);
            wl_list_remove(&redirect_target->link);
            free(redirect_target);
            break;
        }
    }

}

static void
share_surface_redirect_touch_motion(struct wl_client *client,
                                    struct wl_resource *resource,
                                    int32_t id,
                                    wl_fixed_t x,
                                    wl_fixed_t y)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_compositor *compositor = shell_ext->wc;
    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);
    struct weston_seat *seat = NULL;
    struct wl_resource *target_resource = NULL;
    uint32_t time = get_event_time();
    struct wl_resource *surface_resource = client_link->parent->surface->resource;

    seat = get_weston_seat(compositor, client_link);
    if (seat == NULL) {
        return;
    }

    wl_list_for_each(target_resource, &seat->touch->resource_list, link) {
        if (wl_resource_get_client(target_resource) == wl_resource_get_client(surface_resource)) {
            wl_touch_send_motion(target_resource, time, id, x, y);
            break;
        }
    }
}

static void
share_surface_redirect_touch_frame(struct wl_client *client,
                                   struct wl_resource *resource)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_compositor *compositor = shell_ext->wc;
    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);
    struct weston_seat *seat = NULL;
    struct wl_resource *target_resource = NULL;
    struct wl_resource *surface_resource = client_link->parent->surface->resource;

    seat = get_weston_seat(compositor, client_link);
    if (seat == NULL) {
        return;
    }

    wl_list_for_each(target_resource, &seat->touch->resource_list, link) {
        if (wl_resource_get_client(target_resource) == wl_resource_get_client(surface_resource)) {
            wl_touch_send_frame(target_resource);
            break;
        }
    }
}

static void
share_surface_redirect_touch_cancel(struct wl_client *client,
                                     struct wl_resource *resource)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_compositor *compositor = shell_ext->wc;
    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);
    struct weston_seat *seat = NULL;
    struct wl_resource *target_resource = NULL;
    uint32_t time = get_event_time();
    struct wl_resource *surface_resource = client_link->parent->surface->resource;

    seat = get_weston_seat(compositor, client_link);
    if (seat == NULL) {
        return;
    }

    wl_list_for_each(target_resource, &seat->touch->resource_list, link) {
        if (wl_resource_get_client(target_resource) == wl_resource_get_client(surface_resource)) {
            wl_touch_send_cancel(target_resource);
            break;
        }
    }
}

static void
nativesurface_destroy(struct wl_listener *listener, void *data)
{
    struct ivi_nativesurface *nativesurf = NULL;
    nativesurf = container_of(listener, struct ivi_nativesurface, surface_destroy_listener);
    if (NULL == nativesurf) {
        return;
    }

    struct weston_surface *surface = data;
    struct ivi_nativesurface *nativesurf_next = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    wl_list_for_each_safe(nativesurf, nativesurf_next, &shell_ext->list_nativesurface, link) {
        if (NULL != nativesurf && nativesurf->surface == surface) {
            remove_nativesurface(nativesurf);
        }
    }
}

static const
struct wl_share_surface_ext_interface share_surface_ext_implementation = {
    share_surface_destroy,
    share_surface_redirect_touch_down,
    share_surface_redirect_touch_up,
    share_surface_redirect_touch_motion,
    share_surface_redirect_touch_frame,
    share_surface_redirect_touch_cancel
};

static struct ivi_nativesurface*
find_nativesurface(uint32_t pid, const char *title)
{
    struct ivi_nativesurface *nativesurf = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    wl_list_for_each(nativesurf, &shell_ext->list_nativesurface, link) {
        if (nativesurf->pid == pid) {
            if (nativesurf->title == NULL) {
                continue;
            }
            if (strcmp(nativesurf->title, title) == 0) {
                return nativesurf;
            }
        }
    }
    return NULL;
}

static void
destroy_client_link(struct wl_resource *resource)
{
    struct ivi_shell_ext *shell = get_instance();
    struct ivi_nativesurface *link;
    struct ivi_nativesurface *next;
    struct ivi_nativesurface_client_link *client_link = wl_resource_get_user_data(resource);

    wl_list_remove(&client_link->link);
    free(client_link);
}

static struct ivi_nativesurface_client_link *
add_nativesurface_client(struct ivi_nativesurface *nativesurface,
                         uint32_t id, struct wl_client *client)
{
    struct ivi_nativesurface_client_link *link = malloc(sizeof(*link));
    if (NULL == link) {
        return NULL;
    }

    link->resource = wl_resource_create(client, &wl_share_surface_ext_interface, 2, id);
    link->client = client;
    link->firstSendConfigureComp = false;
    link->parent = nativesurface;
    link->configure_sent = false;

    wl_resource_set_implementation(link->resource, &share_surface_ext_implementation, link, destroy_client_link);
    wl_list_insert(&nativesurface->client_list, &link->link);
    return link;
}

struct ivi_nativesurface*
alloc_nativesurface(struct weston_surface *surface, uint32_t id, uint32_t pid,
                    const char *title, uint32_t bufferType, uint32_t format)
{
    struct ivi_nativesurface *nativesurface = malloc(sizeof(struct ivi_nativesurface));
    if (NULL == nativesurface)
    {
        return NULL;
    }

    nativesurface->id = id;
    nativesurface->surface = surface;
    nativesurface->bufferType = bufferType;
    nativesurface->name = 0;
    nativesurface->width = 0;
    nativesurface->height = 0;
    nativesurface->stride = 0;
    nativesurface->format = format;
    nativesurface->pid = pid;
    nativesurface->title = title == NULL ? NULL : (char *)strdup(title);
    wl_list_init(&nativesurface->client_list);

    nativesurface->surface_destroy_listener.notify = nativesurface_destroy;
    wl_signal_add(&surface->destroy_signal, &nativesurface->surface_destroy_listener);
    return nativesurface;
}

static uint32_t
get_shared_client_input_caps(struct ivi_nativesurface_client_link *client_link)
{
    uint32_t caps = 0;
    struct ivi_shell_ext *shell_ext = get_instance();
    struct weston_seat *seat = get_weston_seat(shell_ext->wc, client_link);
    struct wl_client *creator = wl_resource_get_client(client_link->parent->surface->resource);

    if (seat->touch != NULL) {
        struct wl_resource *resource = NULL;
        wl_list_for_each(resource, &seat->touch->focus_resource_list, link) {
            if (wl_resource_get_client(resource) == creator) {
                caps |= (uint32_t)WL_SHARE_SURFACE_EXT_INPUT_CAPS_TOUCH;
                break;
            }
        }

        wl_list_for_each(resource, &seat->touch->resource_list, link) {
            if (wl_resource_get_client(resource) == creator) {
                caps |= (uint32_t)WL_SHARE_SURFACE_EXT_INPUT_CAPS_TOUCH;
                break;
            }
        }
    }

    return caps;
}

static void
share_get_share_surface(struct wl_client *client, struct wl_resource *resource,
    uint32_t id, uint32_t pid, const char *title)
{
    if (NULL == resource) {
        return;
    }

    struct shell_surface *shsurf = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    struct ivi_nativesurface_client_link *client_link = NULL;
    uint32_t caps = 0;
    wl_list_for_each(shsurf, &shell_ext->list_shell_surface, link) {
        if (pid != shsurf->pid) {
            continue;
        }

        const bool equalNULL = NULL == shsurf->title && NULL == title;
        const bool equalString = NULL != shsurf->title && NULL != title && 0 == strcmp(title, shsurf->title);
        if (!equalNULL && !equalString) {
            continue;
        }

        struct ivi_nativesurface *nativesurf = find_nativesurface(shsurf->pid, title);
        if (NULL == nativesurf) {
            nativesurf = alloc_nativesurface(shsurf->surface, id, pid, title,
                                             (int32_t)WL_SHARE_SURFACE_EXT_TYPE_GBM,
                                             (int32_t)WL_SHARE_SURFACE_EXT_FORMAT_ARGB8888);

            if (NULL == nativesurf) {
                weston_log("Texture Sharing Insufficient memory\n");
                wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                                       "Insufficient memory");
                break;
            }
            wl_list_insert(&shell_ext->list_nativesurface, &nativesurf->link);
        }
        client_link = add_nativesurface_client(nativesurf, id, client);
        caps = get_shared_client_input_caps(client_link);
        wl_share_surface_ext_send_input_capabilities(client_link->resource, caps);

        ivi_layout_surface_set_is_forced_configure_event(nativesurf->surface, true);

        return;
    }
}

static struct wl_share_ext_interface g_share_implementation = {
    share_get_share_surface
};

static void
weston_surface_pong(struct wl_client *client,
        struct wl_resource *resource,
        uint32_t serial)
{
    (void) serial;
}

static void
weston_surface_move(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *seat,
        uint32_t serial)
{
    (void) serial;
}

static void
weston_surface_resize(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *seat,
        uint32_t serial,
        uint32_t edges)
{
    (void) serial;
    (void) edges;
}

static void
weston_surface_set_toplevel(struct wl_client *client,
        struct wl_resource *resource)
{

}

static void
weston_surface_set_transient(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *parent,
        int32_t x,
        int32_t y,
        uint32_t flags)
{
    (void) x;
    (void) y;
    (void) flags;
}

static void
weston_surface_set_fullscreen(struct wl_client *client,
        struct wl_resource *resource,
        uint32_t method,
        uint32_t framerate,
        struct wl_resource *output)
{
    (void) method;
    (void) framerate;
}

static void
weston_surface_set_popup(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *seat,
        uint32_t serial,
        struct wl_resource *parent,
        int32_t x,
        int32_t y,
        uint32_t flags)
{
    (void) serial;
    (void) x;
    (void) y;
    (void) flags;
}

static void
weston_surfade_set_maximized(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *output)
{

}
static void
weston_surface_set_title(struct wl_client *client,
			 struct wl_resource *resource,
		         const char *title)
{
    if (NULL == resource) {
        weston_log("resource is NULL pointer.\n");
        return;
    }

    struct weston_surface *weston_surf = wl_resource_get_user_data(resource);
    struct shell_surface *shsurf = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    wl_list_for_each(shsurf, &shell_ext->list_shell_surface, link) {
        if (shsurf->resource == weston_surf->resource) {
            free(shsurf->title);
            shsurf->title = (char *)strdup(title);
            return;
        }
    }
    weston_log("weston_surface not found.\n");
}

static void
weston_surface_set_class(struct wl_client *client,
        struct wl_resource *resource,
        const char *class_)
{
    (void) *class_;
}

static struct wl_shell_surface_interface g_surface_interface = {
    weston_surface_pong,
    weston_surface_move,
    weston_surface_resize,
    weston_surface_set_toplevel,
    weston_surface_set_transient,
    weston_surface_set_fullscreen,
    weston_surface_set_popup,
    weston_surfade_set_maximized,
    weston_surface_set_title,
    weston_surface_set_class
};

static void
set_shell_surface_implementation(struct wl_client *client,
                                 uint32_t id_wl_shell,
                                 struct shell_surface *shsurf)
{
    assert(client);
    assert(shsurf);

    shsurf->resource = wl_resource_create(client, &wl_shell_surface_interface,
                                          1, id_wl_shell);

    wl_resource_set_implementation(shsurf->resource,
                                   &g_surface_interface,
                                   shsurf,
                                   remove_shell_surface);
}

static struct shell_surface *
create_shell_surface(struct wl_client *client,
                     uint32_t id_wl_shell,
                     struct weston_surface *surface)
{
    struct shell_surface *shsurf = NULL;

    shsurf = calloc(1, sizeof *shsurf);
    if (shsurf == NULL) {
        weston_log("fails to allocate memory\n");
        return NULL;
    }

    shsurf->surface = surface;
    shsurf->title = strdup("");

    /* init link so its safe to always remove it in destroy_shell_surface */
    wl_list_init(&shsurf->link);
    set_shell_surface_implementation(client, id_wl_shell, shsurf);

    return shsurf;
}

static void
shell_get_shell_surface(struct wl_client   *client,
                        struct wl_resource *resource,
                        uint32_t id_wl_shell,
                        struct wl_resource *surface_resource)
{
    struct ivi_shell_ext *shell = get_instance();
    struct weston_surface *surface = NULL;
    struct shell_surface  *shsurf  = NULL;

    surface = wl_resource_get_user_data(surface_resource);
    wl_list_for_each(shsurf, &shell->list_shell_surface, link) {
        if (shsurf->surface == surface) {
            set_shell_surface_implementation(client, id_wl_shell, shsurf);
            return;
        }
    }

    shsurf = create_shell_surface(client, id_wl_shell, surface);
    if (!shsurf) {
        wl_resource_post_error(surface_resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface->configure already set");
        return;
    }

    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    shsurf->pid = pid;
    shsurf->resource = resource;
    wl_list_insert(&shell->list_shell_surface, &shsurf->link);
}

static const struct wl_shell_interface shell_implementation = {
    shell_get_shell_surface
};

static void
bind_shell(struct wl_client *client, void *data,
           uint32_t version, uint32_t id)
{
    struct ivi_shell_ext *shell = get_instance();
    struct wl_resource *resource = NULL;
    resource = wl_resource_create(client, &wl_shell_interface, 1, id);
    wl_resource_set_implementation(resource,
                                   &shell_implementation,
                                   shell, NULL);
}

static uint32_t
init_ivi_shell_ext(struct weston_compositor *wc)
{
    struct ivi_shell_ext *shell = get_instance();

    shell->wc = wc;
    wl_list_init(&shell->list_shell_surface);

    shell->destroy_listener.notify = ivi_shell_ext_destroy;
    wl_signal_add(&wc->destroy_signal, &shell->destroy_listener);

    if (wl_global_create(wc->wl_display, &wl_shell_interface, 1,
                         shell, bind_shell) == NULL) {
        return -1;
    }

    return 0;
}

static void
send_configure(struct wl_resource *p_resource, uint32_t id,
               struct ivi_nativesurface *p_nativesurface)
{
    if ((NULL != p_resource) && (NULL != p_nativesurface)) {
        wl_share_surface_ext_send_configure(p_resource, id,
                                            p_nativesurface->bufferType,
                                            p_nativesurface->width,
                                            p_nativesurface->height,
                                            p_nativesurface->stride / 4,
                                            p_nativesurface->format);
    }
}

static void
send_update(struct wl_resource *p_resource, uint32_t id,
            struct ivi_nativesurface *p_nativesurface)
{
    if ((NULL != p_resource) && (NULL != p_nativesurface)) {
        wl_share_surface_ext_send_update(p_resource, id,
                                         p_nativesurface->name);
    }
}

void
send_configure_to_client(struct weston_surface *surface)
{
    struct ivi_shell_ext *shell = get_instance();
    struct ivi_nativesurface *link;
    struct ivi_nativesurface_client_link *client_link;
    struct redirect_target *redirect_link;

    wl_list_for_each(link, &shell->list_nativesurface, link) {
        if (link->surface != surface) {
            continue;
        }

        wl_list_for_each(client_link, &link->client_list, link) {
            if (client_link->configure_sent) {
                send_update(client_link->resource, link->id, link);
            }
            else {
                send_configure(client_link->resource, link->id, link);
                client_link->configure_sent = true;
            }
        }
    }
}

static void
send_to_client(struct ivi_nativesurface *p_nativesurface, uint32_t send_flag)
{
    struct ivi_nativesurface_client_link *p_link = NULL;
    wl_list_for_each(p_link, &p_nativesurface->client_list, link)
    {
        if ((!p_link->firstSendConfigureComp) ||
            (IVI_SHARESURFACE_CONFIGURE & send_flag) == IVI_SHARESURFACE_CONFIGURE) {
            send_configure(p_link->resource, p_nativesurface->id,
                           p_nativesurface);
            p_link->firstSendConfigureComp = true;
        }
        if ((IVI_SHARESURFACE_UPDATE & send_flag) == IVI_SHARESURFACE_UPDATE) {
            send_update(p_link->resource, p_nativesurface->id,
                        p_nativesurface);
        }
    }
}

static uint32_t
update_nativesurface(struct ivi_nativesurface *p_nativesurface)
{
    if (NULL == p_nativesurface || NULL == p_nativesurface->surface) {
        return IVI_SHARESURFACE_STABLE;
    }

    struct ivi_shell_ext *shell_ext = get_instance();
    struct drm_compositor *dc = (struct drm_compositor *)shell_ext->wc;
    struct weston_buffer *buf = p_nativesurface->surface->buffer_ref.buffer;
    if (NULL == buf) {
        return IVI_SHARESURFACE_STABLE;
    }

    struct gbm_bo *bo = gbm_bo_import(dc->gbm, GBM_BO_IMPORT_WL_BUFFER,
                                      buf->legacy_buffer, GBM_BO_USE_SCANOUT);
    if (NULL == bo) {
        weston_log("Texture Sharing Failed to import gbm_bo\n");
        return IVI_SHARESURFACE_STABLE;
    }

    struct drm_gem_flink flink = {};
    flink.handle = gbm_bo_get_handle(bo).u32;
    if (0 != drmIoctl(gbm_device_get_fd(dc->gbm), DRM_IOCTL_GEM_FLINK, &flink)) {
        weston_log("Texture Sharing gem_flink: returned non-zero failedi\n");
        return IVI_SHARESURFACE_STABLE;
    }

    uint32_t name = flink.name;
    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);

    uint32_t ret = IVI_SHARESURFACE_STABLE;
    if (name != p_nativesurface->name) {
        ret |= IVI_SHARESURFACE_UPDATE;
    }
    if (width != p_nativesurface->width) {
        ret |= IVI_SHARESURFACE_CONFIGURE;
    }
    if (height != p_nativesurface->height) {
        ret |= IVI_SHARESURFACE_CONFIGURE;
    }
    if (stride != p_nativesurface->stride) {
        ret |= IVI_SHARESURFACE_CONFIGURE;
    }

    p_nativesurface->name = name;
    p_nativesurface->width = width;
    p_nativesurface->height = height;
    p_nativesurface->stride = stride;

    gbm_bo_destroy(bo);

    return ret;
}

static void
add_shell_surface_in_ivi_shell_ext(struct weston_surface *surface)
{
    struct shell_surface *s = NULL;
    struct ivi_shell_ext *shell_ext = get_instance();
    wl_list_for_each(s, &shell_ext->list_shell_surface, link) {
        if (s->surface == surface) {
            return;
        }
    }

    struct shell_surface *shsurf = NULL;
    shsurf = malloc(sizeof *shsurf);
    if (NULL == shsurf || NULL == surface)
    {
        return;
    }

    struct wl_client *client = wl_resource_get_client(surface->resource);
    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    shsurf->resource = surface->resource;
    shsurf->pid = pid;
    shsurf->surface = surface;
    shsurf->title = NULL;
    wl_list_init(&shsurf->link);
    wl_list_insert(shell_ext->list_shell_surface.prev, &shsurf->link);
}

struct update_sharing_surface_content {
    struct wl_listener listener;
};

struct add_weston_surface_data {
    struct wl_listener create_surface_listener;
    struct weston_surface *surface;
};

static void
send_nativesurface_event(struct wl_listener *listener, void *data)
{
    struct ivi_shell_ext *shell_ext = get_instance();
    if (wl_list_empty(&shell_ext->list_nativesurface)) {
        return;
    }

    struct ivi_nativesurface *p_nativesurface = NULL;
    struct ivi_nativesurface *p_next = NULL;
    wl_list_for_each_safe(p_nativesurface, p_next, &shell_ext->list_nativesurface, link)
    {
        if (NULL == p_nativesurface) {
            continue;
        }
        if (wl_list_empty(&p_nativesurface->client_list) ||
            (0 == p_nativesurface->id))
        {
            weston_log("Texture Sharing warning, Unnecessary nativesurface exists.");
            wl_list_remove(&p_nativesurface->link);
            free_nativesurface(p_nativesurface);
            continue;
        }
        uint32_t send_flag = update_nativesurface(p_nativesurface);
        send_to_client(p_nativesurface, send_flag);
    }
}

static void
send_add_weston_surf_data(struct wl_listener *listener, void *data)
{
    struct weston_surface *ws = data;
    struct add_weston_surface_data *surf_mgr = NULL;
    surf_mgr = container_of(listener, struct add_weston_surface_data, create_surface_listener);
    surf_mgr->surface = ws;
    add_shell_surface_in_ivi_shell_ext(surf_mgr->surface);
}

static void
bind_share_interface(struct wl_client *p_client, void *p_data,
                     uint32_t version, uint32_t id)
{
    weston_log("Texture Saharing Request bind: version(%d)\n", version);

    if (NULL == p_client) {
        return;
    }

    struct ivi_shell_ext *shell_ext = get_instance();
    shell_ext->resource =
        wl_resource_create(p_client, &wl_share_ext_interface, version, id);
    wl_resource_set_implementation(shell_ext->resource,
                                   &g_share_implementation,
                                   shell_ext, NULL);
}

static int32_t
texture_sharing_init(struct weston_compositor *wc)
{
    if (NULL == wc) {
        weston_log("Can not execute texture sharing.\n");
        return -1;
    }

    struct ivi_shell_ext *shell_ext = get_instance();
    if (NULL == wl_global_create(wc->wl_display, &wl_share_ext_interface, 2,
                                 shell_ext, bind_share_interface)) {
        weston_log("Texture Sharing, Failed to global create\n");
        return -1;
    }
    return 0;
}

int32_t
setup_texture_sharing(struct weston_compositor *wc)
{
    if (NULL == wc) {
        weston_log("Can not execute texture sharing.\n");
        return -1;
    }

    int32_t init_ext_ret = init_ivi_shell_ext(wc);
    if (init_ext_ret < 0) {
        weston_log("ivi_shell_ext initialize failed. init_ext_ret = %d\n", init_ext_ret);
        return init_ext_ret;
    }

    int32_t init_ret = texture_sharing_init(wc);
    if (init_ret < 0) {
        weston_log("Texture Sharing initialize failed. init_ret = %d\n", init_ret);
        return init_ret;
    }

    struct weston_output *output = NULL;
    struct update_sharing_surface_content *surf_content = NULL;
    struct add_weston_surface_data *weston_surf_data = NULL;

    surf_content = malloc(sizeof *surf_content);
    if (surf_content == NULL) {
        weston_log("Texture Sharing can't use.\n");
        return -1;
    }

    weston_surf_data = malloc(sizeof *weston_surf_data);
    if (weston_surf_data == NULL) {
        weston_log("weston_surf_data can't create.\n");
        free(surf_content);
        return -1;
    }

    wl_list_for_each(output, &wc->output_list, link) {
        surf_content->listener.notify = send_nativesurface_event;
        wl_signal_add(&output->frame_signal, &surf_content->listener);
    }

    weston_surf_data->create_surface_listener.notify = send_add_weston_surf_data;
    wl_signal_add(&wc->create_surface_signal, &weston_surf_data->create_surface_listener);
    return 0;
}
#endif
