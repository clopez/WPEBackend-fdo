#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/view-backend.h>

typedef void* EGLDisplay;
typedef void* EGLImageKHR;

enum wpe_view_backend_buffer_type {
    WPE_VIEW_BACKEND_BUFFER_TYPE_NONE,
    WPE_VIEW_BACKEND_BUFFER_TYPE_WL_RESOURCE,
    WPE_VIEW_BACKEND_BUFFER_TYPE_LINUX_DMABUF,
};

struct wl_resource;
struct wpe_view_backend_exportable_fdo;

/**
 * wpe_view_backend_exportable_fdo_buffer:
 *
 * An opaque structure that encapsulates information about a buffer
 * delivered to the application for rendering.
 *
 * Since the introduction of linux-dmabuf interface, there are now two
 * types of buffer that can be delivered: wayland buffer resource, and
 * linux-dmabuf buffer descriptors.
 *
 * This structure, together with the single 'export_buffer' interface
 * callback abstracts the underlying nature of the buffer to
 * applications.
 */
struct wpe_view_backend_exportable_fdo_buffer;

struct wpe_view_backend_exportable_fdo_client {
    /**
     * Applications hook into 'export_buffer' callback to retrieve
     * buffers that are ready for rendering. The @buffer argument
     * is an abstract object whose type is @buffer_type.
     *
     * Applications can then use any of the 'accessor' methods to
     * retrieve platform-specific contents of the buffer.
     *
     * Applications must release the buffer when it is no longer needed,
     * by calling wpe_view_backend_exportable_fdo_dispatch_release_buffer().
     */
    void (*export_buffer)(void* data, enum wpe_view_backend_buffer_type buffer_type,
                          struct wpe_view_backend_exportable_fdo_buffer *buffer);
};

struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_create(struct wpe_view_backend_exportable_fdo_client*, void*, uint32_t width, uint32_t height);

void
wpe_view_backend_exportable_fdo_destroy(struct wpe_view_backend_exportable_fdo*);

struct wpe_view_backend*
wpe_view_backend_exportable_fdo_get_view_backend(struct wpe_view_backend_exportable_fdo*);

void
wpe_view_backend_exportable_fdo_dispatch_frame_complete(struct wpe_view_backend_exportable_fdo*);

void
wpe_view_backend_exportable_fdo_dispatch_release_buffer(struct wpe_view_backend_exportable_fdo*, struct wpe_view_backend_exportable_fdo_buffer *buffer);

/**
 * wpe_view_backend_exportable_fdo_get_egl_image:
 *
 * Import the given opaque buffer into an EGLImageKHR.
 *
 * This method will choose the proper EGL attributes to use, based on
 * the type of buffer (wl_resource or linux-dmabuf).
 *
 * Returns: An EGLImageKHR, which must be destroyed with
 * eglDestroyImageKHR(); or NULL on error, in which case
 * eglGetError() can be used to retrieve it.
 */
EGLImageKHR
wpe_view_backend_exportable_fdo_get_egl_image(struct wpe_view_backend_exportable_fdo*,
                                              struct wpe_view_backend_exportable_fdo_buffer*,
                                              EGLDisplay);
#ifdef __cplusplus
}
#endif
