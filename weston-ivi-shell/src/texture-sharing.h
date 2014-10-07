#ifdef USE_DRM
#ifndef _TEXTURE_SHARING_H_
#define _TEXTURE_SHARING_H_

void cleanup_texture_sharing(struct wl_client *client);
int setup_texture_sharing(struct weston_compositor *wc);

#endif
#endif
