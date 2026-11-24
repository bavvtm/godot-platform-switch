#pragma once

#ifdef GLES3_ENABLED

#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/math/vector2i.h"
#include "core/templates/hash_map.h"
#include "servers/display/display_server.h"
#include "servers/display/display_server_enums.h"

struct NWindow;

#include <EGL/eglplatform.h>
typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef void *EGLConfig;
typedef int32_t EGLint;

class GLManager_Switch {
public:
	struct GLWindow {
		DisplayServerEnums::WindowID window_id = DisplayServerEnums::INVALID_WINDOW_ID;
		NWindow *nwindow = nullptr;
		EGLSurface egl_surface = nullptr;
		Point2i size = Point2i(1280, 720);
		bool in_use = false;
	};

private:
	EGLDisplay display = nullptr;
	EGLContext context = nullptr;
	EGLConfig config = nullptr;

	DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
	HashMap<DisplayServerEnums::WindowID, GLWindow> windows;

	static const char *egl_error_to_string(EGLint p_error);
	static void check_egl_error(const char *p_step);

	Error _create_window_surface(GLWindow &p_win);

public:
	Error initialize();

	Error window_create(DisplayServerEnums::WindowID p_window_id, NWindow *p_native_window, const Point2i &p_size = Point2i(1280, 720));
	void window_destroy(DisplayServerEnums::WindowID p_window_id);
	void window_resize(DisplayServerEnums::WindowID p_window_id, const Point2i &p_size);

	Point2i window_get_size(DisplayServerEnums::WindowID p_window_id) const;

	void make_current();
	void release_current();
	void swap_buffers();

	void set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window_id = DisplayServerEnums::MAIN_WINDOW_ID);
	DisplayServerEnums::VSyncMode get_vsync_mode() const { return vsync_mode; }
	bool is_using_vsync() const { return vsync_mode == DisplayServerEnums::VSYNC_ENABLED; }

	_FORCE_INLINE_ EGLDisplay get_display() const { return display; }
	_FORCE_INLINE_ EGLContext get_context() const { return context; }

	GLManager_Switch();
	~GLManager_Switch();
};

#endif // GLES3_ENABLED
