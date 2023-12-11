#ifndef LIBWAYLAND_SHIM_H
#define LIBWAYLAND_SHIM_H

#include <wayland-client-core.h>
#include <glib.h>

#define LIBWAYLAND_SHIM_DISPATCH_CLIENT_EVENT(listener, proxy, event, ...) \
    if (((struct wl_proxy*)proxy)->object.implementation) { \
        ((struct listener *)((struct wl_proxy*)proxy)->object.implementation)->event( \
            ((struct wl_proxy*)proxy)->user_data, __VA_ARGS__); }

// From wayland-private.h in libwayland
struct wl_object {
	const struct wl_interface *interface;
	const void *implementation;
	uint32_t id;
};

// From wayland-client.c in libwayland
struct wl_proxy {
	struct wl_object object;
	struct wl_display *display;
	struct wl_event_queue *queue;
	uint32_t flags;
	int refcount;
	void *user_data;
	wl_dispatcher_func_t dispatcher;
	uint32_t version;
	const char * const *tag;
	struct wl_list queue_link; // appears in wayland 1.22
};

typedef struct wl_proxy *(*libwayland_shim_client_proxy_handler_func_t) (
    void* data,
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args);

typedef void (*libwayland_shim_client_proxy_destroy_func_t) (
    void* data,
    struct wl_proxy *proxy);

gboolean libwayland_shim_has_initialized ();

struct wl_proxy *libwayland_shim_create_client_proxy (
    struct wl_proxy *factory,
    const struct wl_interface *interface,
    uint32_t version,
    libwayland_shim_client_proxy_handler_func_t handler,
    libwayland_shim_client_proxy_destroy_func_t destroy,
    void* data);

void libwayland_shim_clear_client_proxy_data (struct wl_proxy *proxy);

void *libwayland_shim_get_client_proxy_data (struct wl_proxy *proxy, void* expected_handler);

extern struct wl_proxy * (*libwayland_shim_real_wl_proxy_marshal_array_flags) (
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args);

#endif // LIBWAYLAND_SHIM_H
