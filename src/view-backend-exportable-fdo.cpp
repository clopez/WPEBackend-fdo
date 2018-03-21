#include <wpe-fdo/view-backend-exportable.h>

#include <assert.h>
#include "ws.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>

extern "C" {

struct wpe_view_backend_exportable_fdo_buffer {
    struct wl_resource *wlResourceBuffer;
    const struct linux_dmabuf_buffer *dmabufBuffer;
};

}

namespace {

class ViewBackend;

struct ClientBundle {
    struct wpe_view_backend_exportable_fdo_client* client;
    void* data;
    ViewBackend* viewBackend;
    uint32_t initialWidth;
    uint32_t initialHeight;
};

class ViewBackend : public WS::ExportableClient {
public:
    ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
        : m_clientBundle(clientBundle)
        , m_backend(backend)
    {
        m_clientBundle->viewBackend = this;
    }

    ~ViewBackend()
    {
        if (m_clientFd != -1)
            close(m_clientFd);

        if (m_source) {
            g_source_destroy(m_source);
            g_source_unref(m_source);
        }
        if (m_socket)
            g_object_unref(m_socket);
    }

    void initialize()
    {
        int sockets[2];
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
        if (ret == -1)
            return;

        m_socket = g_socket_new_from_fd(sockets[0], nullptr);
        if (!m_socket) {
            close(sockets[0]);
            close(sockets[1]);
            return;
        }

        g_socket_set_blocking(m_socket, FALSE);

        m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
        g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(s_socketCallback), this, nullptr);
        g_source_set_priority(m_source, -70);
        g_source_set_can_recurse(m_source, TRUE);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_clientFd = sockets[1];

        wpe_view_backend_dispatch_set_size(m_backend,
            m_clientBundle->initialWidth, m_clientBundle->initialHeight);
    }

    int clientFd()
    {
        return dup(m_clientFd);
    }

    void frameCallback(struct wl_resource* callbackResource) override
    {
        m_callbackResources.push_back(callbackResource);
    }

    void exportBufferResource(struct wl_resource* bufferResource) override
    {
        struct wpe_view_backend_exportable_fdo_buffer *buffer = new(struct wpe_view_backend_exportable_fdo_buffer);
        buffer->wlResourceBuffer = bufferResource;
        buffer->dmabufBuffer = nullptr;
        m_clientBundle->client->export_buffer(m_clientBundle->data,
                                              WPE_VIEW_BACKEND_BUFFER_TYPE_WL_RESOURCE,
                                              buffer);
    }

    void exportLinuxDmabuf(const struct linux_dmabuf_buffer *dmabuf_buffer) override
    {
        struct wpe_view_backend_exportable_fdo_buffer *buffer = new(struct wpe_view_backend_exportable_fdo_buffer);
        buffer->wlResourceBuffer = nullptr;
        buffer->dmabufBuffer = dmabuf_buffer;
        m_clientBundle->client->export_buffer(m_clientBundle->data,
                                              WPE_VIEW_BACKEND_BUFFER_TYPE_LINUX_DMABUF,
                                              buffer);
    }

    void dispatchFrameCallback()
    {
        for (auto* resource : m_callbackResources)
            wl_callback_send_done(resource, 0);
        m_callbackResources.clear();
    }

    void releaseBuffer(const struct wpe_view_backend_exportable_fdo_buffer *buffer)
    {
        if (buffer->wlResourceBuffer)
            wl_buffer_send_release(buffer->wlResourceBuffer);
        delete(buffer);
    }

private:
    static gboolean s_socketCallback(GSocket*, GIOCondition, gpointer);

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    std::vector<struct wl_resource*> m_callbackResources;

    GSocket* m_socket;
    GSource* m_source;
    int m_clientFd { -1 };
};

gboolean ViewBackend::s_socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    uint32_t message[2];
    gssize len = g_socket_receive(socket, reinterpret_cast<gchar*>(message), sizeof(uint32_t) * 2,
        nullptr, nullptr);
    if (len == -1)
        return FALSE;

    if (len == sizeof(uint32_t) * 2 && message[0] == 0x42) {
        auto& viewBackend = *static_cast<ViewBackend*>(data);
        WS::Instance::singleton().registerViewBackend(message[1], viewBackend);
    }

    return TRUE;
}

static struct wpe_view_backend_interface view_backend_exportable_fdo_interface = {
    // create
    [](void* data, struct wpe_view_backend* backend) -> void*
    {
        auto* clientBundle = reinterpret_cast<ClientBundle*>(data);
        return new ViewBackend(clientBundle, backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = reinterpret_cast<ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        return backend.clientFd();
    }
};

} // namespace

extern "C" {

struct wpe_view_backend_exportable_fdo {
    ClientBundle* clientBundle;
    struct wpe_view_backend* backend;
};

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_create(struct wpe_view_backend_exportable_fdo_client* client, void* data, uint32_t width, uint32_t height)
{
    auto* clientBundle = new ClientBundle{ client, data, nullptr, width, height };

    struct wpe_view_backend* backend = wpe_view_backend_create_with_backend_interface(&view_backend_exportable_fdo_interface, clientBundle);

    auto* exportable = new struct wpe_view_backend_exportable_fdo;
    exportable->clientBundle = clientBundle;
    exportable->backend = backend;

    return exportable;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_destroy(struct wpe_view_backend_exportable_fdo* exportable)
{
    wpe_view_backend_destroy(exportable->backend);
    delete exportable;
}

__attribute__((visibility("default")))
struct wpe_view_backend*
wpe_view_backend_exportable_fdo_get_view_backend(struct wpe_view_backend_exportable_fdo* exportable)
{
    return exportable->backend;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_dispatch_frame_complete(struct wpe_view_backend_exportable_fdo* exportable)
{
    exportable->clientBundle->viewBackend->dispatchFrameCallback();
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_dispatch_release_buffer(struct wpe_view_backend_exportable_fdo* exportable,
                                                        struct wpe_view_backend_exportable_fdo_buffer *buffer)
{
    exportable->clientBundle->viewBackend->releaseBuffer(buffer);
}

__attribute__((visibility("default")))
EGLImageKHR
wpe_view_backend_exportable_fdo_get_egl_image(struct wpe_view_backend_exportable_fdo* exportable,
                                              struct wpe_view_backend_exportable_fdo_buffer *buffer,
                                              EGLDisplay display)
{
    EGLint attribs[50];
    int atti = 0;
    EGLint import_type;
    EGLClientBuffer client_buffer = nullptr;

    if (buffer->dmabufBuffer) {
        const struct linux_dmabuf_attributes *buf_attribs =
            linux_dmabuf_get_buffer_attributes(buffer->dmabufBuffer);

        import_type = EGL_LINUX_DMA_BUF_EXT;

        attribs[atti++] = EGL_WIDTH;
        attribs[atti++] = buf_attribs->width;
        attribs[atti++] = EGL_HEIGHT;
        attribs[atti++] = buf_attribs->height;
        attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
        attribs[atti++] = buf_attribs->format;

        for (int i = 0; i < buf_attribs->n_planes; i++) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attribs[atti++] = buf_attribs->fd[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attribs[atti++] = buf_attribs->offset[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attribs[atti++] = buf_attribs->stride[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = buf_attribs->modifier[i] & 0xFFFFFFFF;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = buf_attribs->modifier[i] >> 32;
        }
    } else {
        assert(buffer->wlResourceBuffer);

        import_type = EGL_WAYLAND_BUFFER_WL;
        client_buffer = buffer->wlResourceBuffer;

        attribs[atti++] = EGL_WAYLAND_PLANE_WL;
        attribs[atti++] = 0;
    }

    attribs[atti++] = EGL_NONE;

    static PFNEGLCREATEIMAGEKHRPROC eglCreateImage = NULL;
    if (eglCreateImage == NULL) {
        eglCreateImage = (PFNEGLCREATEIMAGEKHRPROC)
            eglGetProcAddress ("eglCreateImageKHR");
        assert (eglCreateImage != NULL);
    }

    return eglCreateImage (display,
                           EGL_NO_CONTEXT,
                           import_type,
                           client_buffer,
                           attribs);
}

const struct linux_dmabuf_attributes*
wpe_view_backend_exportable_fdo_get_dmabuf_attributes(struct wpe_view_backend_exportable_fdo *exportable,
                                                      struct wpe_view_backend_exportable_fdo_buffer *buffer)
{
    if (!buffer->dmabufBuffer)
        return NULL;

    return linux_dmabuf_get_buffer_attributes(buffer->dmabufBuffer);
}

}
