#ifndef _XFWM_LAYERS_H
#define _XFWM_LAYERS_H
#include <wlr/types/wlr_layer_shell_v1.h>

struct _xfwmServer;
typedef struct _xfwmServer xfwmServer;

struct _Output;
typedef struct _Output Output;

struct _xfwmLayerSurface {
	Output *output;
	xfwmServer *server;

	struct wlr_scene_layer_surface_v1 *scene;

	bool mapped;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener new_popup;
};

typedef struct _xfwmLayerSurface xfwmLayerSurface;

struct _xfwmLayerPopup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene;

	struct wl_listener destroy;
	struct wl_listener new_popup;
};

typedef struct _xfwmLayerPopup xfwmLayerPopup;

struct _xfwmLayerSubsurface {
	struct wlr_scene_tree *scene;

	struct wl_listener destroy;
};

typedef struct _xfwmLayerSubsurface xfwmLayerSubsurface;

enum SceneDescriptorType {
	XFWM_SCENE_DESC_NODE,
	XFWM_SCENE_DESC_LAYER_SHELL,
	XFWM_SCENE_DESC_LAYER_SHELL_POPUP,
};

struct _xfwmSceneDescriptor {
	enum SceneDescriptorType type;
	void *data;
	struct wl_listener destroy;
};

typedef struct _xfwmSceneDescriptor xfwmSceneDescriptor;

void init_layer_shell(xfwmServer *server);
void assign_scene_descriptor(struct wlr_scene_node *node,
	enum SceneDescriptorType type, void *data);

#endif
