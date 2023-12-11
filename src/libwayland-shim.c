#include <dlfcn.h>
#include <stdlib.h>
#include "libwayland-shim.h"
#include "layer-surface.h"
#include "wayland-utils.h"

struct wl_proxy *(*libwayland_shim_real_wl_proxy_marshal_array_flags) (
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args) = NULL;

void (*libwayland_shim_real_wl_proxy_destroy) (struct wl_proxy *proxy) = NULL;

int (*libwayland_shim_real_wl_proxy_add_dispatcher)(struct wl_proxy *proxy,
			wl_dispatcher_func_t dispatcher_func,
			const void * dispatcher_data, void *data) = NULL;

gboolean
libwayland_shim_has_initialized ()
{
    return libwayland_shim_real_wl_proxy_marshal_array_flags != NULL;
}

static void
libwayland_shim_init ()
{
    if (libwayland_shim_has_initialized ())
        return;

    void *handle = dlopen("libwayland-client.so", RTLD_LAZY);
    if (handle == NULL) {
        g_error ("failed to dlopen libwayland");
    }

#define INIT_SYM(name) if (!(libwayland_shim_real_##name = dlsym(handle, #name))) {\
    g_error ("dlsym failed to load %s", #name); }

    INIT_SYM(wl_proxy_marshal_array_flags);
    INIT_SYM(wl_proxy_destroy);
    INIT_SYM(wl_proxy_add_dispatcher);

#undef INIT_SYM

    //dlclose(handle);
}

// From wayland-private.h in libwayland
#define WL_CLOSURE_MAX_ARGS 20

// From wayland-private.h in libwayland
struct argument_details {
	char type;
	int nullable;
};

// From connection.c in libwayland
static const char *
get_next_argument(const char *signature, struct argument_details *details)
{
	details->nullable = 0;
	for(; *signature; ++signature) {
		switch(*signature) {
		case 'i':
		case 'u':
		case 'f':
		case 's':
		case 'o':
		case 'n':
		case 'a':
		case 'h':
			details->type = *signature;
			return signature + 1;
		case '?':
			details->nullable = 1;
		}
	}
	details->type = '\0';
	return signature;
}

// From connection.c in libwayland
static void
wl_argument_from_va_list(const char *signature, union wl_argument *args,
			 int count, va_list ap)
{
	int i;
	const char *sig_iter;
	struct argument_details arg;

	sig_iter = signature;
	for (i = 0; i < count; i++) {
		sig_iter = get_next_argument(sig_iter, &arg);

		switch(arg.type) {
		case 'i':
			args[i].i = va_arg(ap, int32_t);
			break;
		case 'u':
			args[i].u = va_arg(ap, uint32_t);
			break;
		case 'f':
			args[i].f = va_arg(ap, wl_fixed_t);
			break;
		case 's':
			args[i].s = va_arg(ap, const char *);
			break;
		case 'o':
			args[i].o = va_arg(ap, struct wl_object *);
			break;
		case 'n':
			args[i].o = va_arg(ap, struct wl_object *);
			break;
		case 'a':
			args[i].a = va_arg(ap, struct wl_array *);
			break;
		case 'h':
			args[i].h = va_arg(ap, int32_t);
			break;
		case '\0':
			return;
		}
	}
}

struct wrapped_proxy {
    struct wl_proxy proxy;
    libwayland_shim_client_proxy_handler_func_t handler;
    libwayland_shim_client_proxy_destroy_func_t destroy;
    void* data;
};

// The ID for ALL proxies that are created by us and not managed by the real libwayland
const uint32_t client_facing_proxy_id = 6942069;

struct wl_proxy *
libwayland_shim_create_client_proxy (
    struct wl_proxy *factory,
    const struct wl_interface *interface,
    uint32_t version,
    libwayland_shim_client_proxy_handler_func_t handler,
    libwayland_shim_client_proxy_destroy_func_t destroy,
    void* data)
{
    struct wrapped_proxy* allocation = g_new0 (struct wrapped_proxy, 1);
    g_assert (allocation);
    allocation->proxy.object.interface = interface;
    allocation->proxy.display = factory->display;
    allocation->proxy.queue = factory->queue;
    allocation->proxy.refcount = 1;
    allocation->proxy.version = version;
    allocation->proxy.object.id = client_facing_proxy_id;
    wl_list_init(&allocation->proxy.queue_link);
    allocation->handler = handler;
    allocation->destroy = destroy;
    allocation->data = data;
    return &allocation->proxy;
}

void
libwayland_shim_clear_client_proxy_data (struct wl_proxy *proxy)
{
    if (!proxy) {
        return;
    }
    g_assert(proxy->object.id == client_facing_proxy_id);
    struct wrapped_proxy *wrapper = (struct wrapped_proxy *)proxy;
    wrapper->data = NULL;
    wrapper->destroy = NULL;
    wrapper->handler = NULL;
}

void *
libwayland_shim_get_client_proxy_data (struct wl_proxy *proxy, void* expected_handler)
{
    if (proxy && proxy->object.id == client_facing_proxy_id) {
        struct wrapped_proxy *wrapper = (struct wrapped_proxy *)proxy;
        if (wrapper->handler == expected_handler) {
            return wrapper->data;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

// Overrides the function in wayland-client.c in libwayland
void
wl_proxy_destroy (struct wl_proxy *proxy)
{
    libwayland_shim_init ();
    if (proxy->object.id == client_facing_proxy_id) {
        struct wrapped_proxy *wrapper = (struct wrapped_proxy *)proxy;
        if (wrapper->destroy) {
            wrapper->destroy(wrapper->data, proxy);
        }
        wl_list_remove(&proxy->queue_link);
        // No need to worry about the refcount since it's only accessibly within libwayland, and it's only used by
        // functions that never see client facing objects
        g_free (proxy);
    } else {
        libwayland_shim_real_wl_proxy_destroy (proxy);
    }
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_array_flags (
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args)
{
    libwayland_shim_init ();
    if (proxy->object.id == client_facing_proxy_id) {
        struct wrapped_proxy *wrapper = (struct wrapped_proxy *)proxy;
        struct wl_proxy *result = NULL;
        if (wrapper->handler)
            result = wrapper->handler(wrapper->data, proxy, opcode, interface, version, flags, args);
        if (flags & WL_MARSHAL_FLAG_DESTROY)
            wl_proxy_destroy(proxy);
        return result;
    } else {
        return layer_surface_handle_request (proxy, opcode, interface, version, flags, args);
    }
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t opcode,
		       const struct wl_interface *interface,
		       uint32_t version,
		       uint32_t flags, ...)
{
    union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, flags);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	return wl_proxy_marshal_array_flags(proxy, opcode, interface, version, flags, args);
}

// Overrides the function in wayland-client.c in libwayland
void
wl_proxy_marshal(struct wl_proxy *proxy, uint32_t opcode, ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, opcode);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	wl_proxy_marshal_array_constructor(proxy, opcode, args, NULL);
}

// Overrides the function in wayland-client.c in libwayland
void
wl_proxy_marshal_array(struct wl_proxy *proxy, uint32_t opcode,
		       union wl_argument *args)
{
	wl_proxy_marshal_array_constructor(proxy, opcode, args, NULL);
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_constructor(struct wl_proxy *proxy,
			     uint32_t opcode,
			     const struct wl_interface *interface,
			     ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, interface);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	return wl_proxy_marshal_array_constructor(proxy, opcode,
						  args, interface);
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_constructor_versioned(struct wl_proxy *proxy,
				       uint32_t opcode,
				       const struct wl_interface *interface,
				       uint32_t version,
				       ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, version);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	return wl_proxy_marshal_array_constructor_versioned(proxy, opcode,
							    args, interface,
							    version);
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_array_constructor(struct wl_proxy *proxy,
				   uint32_t opcode, union wl_argument *args,
				   const struct wl_interface *interface)
{
	return wl_proxy_marshal_array_constructor_versioned(proxy, opcode,
							    args, interface,
							    proxy->version);
}

// Overrides the function in wayland-client.c in libwayland
struct wl_proxy *
wl_proxy_marshal_array_constructor_versioned(struct wl_proxy *proxy,
					     uint32_t opcode,
					     union wl_argument *args,
					     const struct wl_interface *interface,
					     uint32_t version)
{
	return wl_proxy_marshal_array_flags(proxy, opcode, interface, version, 0, args);
}

// Overrides the function in wayland-client.c in libwayland
int
wl_proxy_add_dispatcher(struct wl_proxy *proxy,
			wl_dispatcher_func_t dispatcher_func,
			const void * dispatcher_data, void *data)
{
    libwayland_shim_init ();
    if (proxy->object.id == client_facing_proxy_id) {
        g_critical ("wl_proxy_add_dispatcher () not supported for client-facing proxies");
    }
    return libwayland_shim_real_wl_proxy_add_dispatcher(proxy, dispatcher_func, dispatcher_data, data);
}
