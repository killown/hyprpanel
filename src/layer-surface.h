#ifndef LAYER_SHELL_SURFACE_H
#define LAYER_SHELL_SURFACE_H

#include "wlr-layer-shell-unstable-v1-client.h"
#include "gtk4-layer-shell.h"
#include <gtk/gtk.h>

struct wl_surface;
struct xdg_surface;
struct xdg_positioner;

typedef struct _LayerSurface LayerSurface;

// Functions that mutate this structure should all be in layer-surface.c to make the logic easier to understand
// Struct is declared in this header to prevent the need for excess getters
struct _LayerSurface
{
    GtkWindow *gtk_window;

    // Can be set at any time
    gboolean anchors[GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER]; // The current anchor
    int margins[GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER]; // The current margins
    int exclusive_zone; // The current exclusive zone (set either explicitly or automatically)
    gboolean auto_exclusive_zone; // If to automatically change the exclusive zone to match the window size
    GtkLayerShellKeyboardMode keyboard_mode; // Type of keyboard interactivity enabled for this surface
    GtkLayerShellLayer layer; // The current layer, needs surface recreation on old layer shell versions

    // Need the surface to be recreated to change
    GdkMonitor *monitor; // Can be null
    const char *name_space; // Can be null, freed on destruction

    // Not set by user requests
    struct zwlr_layer_surface_v1 *layer_surface; // The actual layer surface Wayland object (can be NULL)
    GtkRequisition cached_xdg_configure_size; // The last size we configured GTK's XDG surface with
    GtkRequisition cached_layer_size_set; // The last size we set the layer surface to with the compositor
    GtkRequisition last_layer_configured_size; // The last size our layer surface received from the compositor
    uint32_t pending_configure_serial; // If non-zero our layer surface received a configure with this serial, we passed
      // it on to GTK's XDG surface and will ack it once GTK acks it's configure. Otherwise this is zero, all acks from
      // GTK can be ignored (they are for configures not originating from the compositor)
    struct xdg_surface *client_facing_xdg_surface;
    struct xdg_toplevel *client_facing_xdg_toplevel;
};

LayerSurface *layer_surface_new (GtkWindow *gtk_window);

LayerSurface *gtk_window_get_layer_surface (GtkWindow *gtk_window);

// Surface is remapped in order to set
void layer_surface_set_monitor (LayerSurface *self, GdkMonitor *monitor); // Can be null for default
void layer_surface_set_name_space (LayerSurface *self, char const* name_space); // Makes a copy of the string, can be null

// Can be set without remapping the surface
void layer_surface_set_layer (LayerSurface *self, GtkLayerShellLayer layer); // Remaps surface on old layer shell versions
void layer_surface_set_anchor (LayerSurface *self, GtkLayerShellEdge edge, gboolean anchor_to_edge);
void layer_surface_set_margin (LayerSurface *self, GtkLayerShellEdge edge, int margin_size);
void layer_surface_set_exclusive_zone (LayerSurface *self, int exclusive_zone);
void layer_surface_auto_exclusive_zone_enable (LayerSurface *self);
void layer_surface_set_keyboard_mode (LayerSurface *self, GtkLayerShellKeyboardMode mode);

// Returns the effective namespace (default if unset). Does not return ownership. Never returns NULL. Handles NULL self.
const char* layer_surface_get_namespace (LayerSurface *self);

// Used by libwayland wrappers
struct wl_proxy *layer_surface_handle_request (
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args);

#endif // LAYER_SHELL_SURFACE_H
