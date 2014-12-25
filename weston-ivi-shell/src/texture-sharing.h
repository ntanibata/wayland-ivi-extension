#ifdef USE_DRM
#ifndef _TEXTURE_SHARING_H_
#define _TEXTURE_SHARING_H_

void cleanup_texture_sharing(struct wl_client *client);
int setup_texture_sharing(struct weston_compositor *wc);
void send_configure_to_client(struct weston_surface *surface);
#endif
#endif
