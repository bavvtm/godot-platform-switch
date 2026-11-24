#include "gl_manager_switch.h"

#ifdef GLES3_ENABLED

#include "switch_wrap.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <thirdparty/glad/glad/gl.h>

#include <stdio.h>

const char *GLManager_Switch::egl_error_to_string(EGLint p_error) {
	switch (p_error) {
		case EGL_SUCCESS:
			return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:
			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG";
		case EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY";
		case EGL_BAD_MATCH:
			return "EGL_BAD_MATCH";
		case EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW";
		case EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER";
		case EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE";
		case EGL_CONTEXT_LOST:
			return "EGL_CONTEXT_LOST";
		default:
			return "UNKNOWN_EGL_ERROR";
	}
}

void GLManager_Switch::check_egl_error(const char *p_step) {
	EGLint err = eglGetError();
	if (err != EGL_SUCCESS) {
		TRACE("EGL failure at step [%s]: %s (0x%04X)", p_step, egl_error_to_string(err), err);
	}
}

Error GLManager_Switch::initialize() {
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY) {
		check_egl_error("eglGetDisplay");
		TRACE("Switch EGL: Failed to get default EGL display.");
		return ERR_CANT_CREATE;
	}

	EGLint major = 0;
	EGLint minor = 0;
	if (!eglInitialize(display, &major, &minor)) {
		check_egl_error("eglInitialize");
		TRACE("Switch EGL: Failed to initialize EGL runtime.");
		display = EGL_NO_DISPLAY;
		return ERR_CANT_CREATE;
	}

	static const EGLint framebuffer_attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};

	EGLint num_configs = 0;
	if (!eglChooseConfig(display, framebuffer_attribs, &config, 1, &num_configs) || num_configs == 0) {
		check_egl_error("eglChooseConfig");
		TRACE("Switch EGL: Framebuffer configuration unsupported.");
		return ERR_CANT_CREATE;
	}

	// request OpenGL ES 3.2
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 2,
		EGL_NONE
	};

	eglBindAPI(EGL_OPENGL_ES_API);
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
	if (context == EGL_NO_CONTEXT) {
		check_egl_error("eglCreateContext");
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error GLManager_Switch::_create_window_surface(GLWindow &p_win) {
	nwindowSetDimensions(p_win.nwindow, (u32)p_win.size.x, (u32)p_win.size.y);

	p_win.egl_surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)p_win.nwindow, nullptr);
	if (p_win.egl_surface == EGL_NO_SURFACE) {
		check_egl_error("eglCreateWindowSurface");
		TRACE("Switch EGL: Failed to create window surface.");
		return ERR_CANT_CREATE;
	}

	if (!eglMakeCurrent(display, p_win.egl_surface, p_win.egl_surface, context)) {
		check_egl_error("eglMakeCurrent");
		TRACE("Switch EGL: Failed to bind context to surface.");
		eglDestroySurface(display, p_win.egl_surface);
		p_win.egl_surface = EGL_NO_SURFACE;
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error GLManager_Switch::window_create(DisplayServerEnums::WindowID p_window_id, NWindow *p_native_window, const Point2i &p_size) {
	if (windows.has(p_window_id)) {
		TRACE("Window ID %d already registered.", (int)p_window_id);
		return ERR_ALREADY_IN_USE;
	}

	if (!p_native_window) {
		TRACE("Native window pointer cannot be null.");
		return ERR_INVALID_PARAMETER;
	}

	GLWindow win;
	win.window_id = p_window_id;
	win.nwindow = p_native_window;
	win.size = p_size;

	Error err = _create_window_surface(win);
	if (err != OK) {
		return err;
	}

	// switch uses native OpenGL ES 3.2 via EGL (instead of desktop OpenGL),
	// so function pointers must be loaded once via gladLoadGLES2 + eglGetProcAddress.
	static bool glad_loaded = false;
	if (!glad_loaded) {
		if (!gladLoadGLES2((GLADloadfunc)eglGetProcAddress)) {
			TRACE("Switch EGL: Failed to load OpenGL ES functions via GLAD.");
			eglDestroySurface(display, win.egl_surface);
			return ERR_CANT_CREATE;
		}
		glad_loaded = true;
	}

	win.in_use = true;
	windows[p_window_id] = win;
	return OK;
}

void GLManager_Switch::window_resize(DisplayServerEnums::WindowID p_window_id, const Point2i &p_size) {
	if (!windows.has(p_window_id)) {
		TRACE("Cannot resize unregistered window ID: %d", (int)p_window_id);
		return;
	}

	GLWindow &win = windows[p_window_id];
	if (win.size == p_size) {
		return;
	}

	win.size = p_size;

	if (win.nwindow && display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (win.egl_surface != EGL_NO_SURFACE) {
			eglDestroySurface(display, win.egl_surface);
			win.egl_surface = EGL_NO_SURFACE;
		}

		if (_create_window_surface(win) == OK) {
			int interval = (vsync_mode == DisplayServerEnums::VSYNC_DISABLED) ? 0 : 1;
			eglSwapInterval(display, interval);
		}
	}
}

Point2i GLManager_Switch::window_get_size(DisplayServerEnums::WindowID p_window_id) const {
	if (!windows.has(p_window_id)) {
		TRACE("Window ID %d not found.", (int)p_window_id);
		return Point2i();
	}
	return windows[p_window_id].size;
}

void GLManager_Switch::window_destroy(DisplayServerEnums::WindowID p_window_id) {
	if (!windows.has(p_window_id)) {
		return;
	}

	GLWindow &win = windows[p_window_id];
	if (win.egl_surface != EGL_NO_SURFACE) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroySurface(display, win.egl_surface);
		win.egl_surface = EGL_NO_SURFACE;
	}

	windows.erase(p_window_id);
}

void GLManager_Switch::make_current() {
	if (windows.has(DisplayServerEnums::MAIN_WINDOW_ID)) {
		GLWindow &win = windows[DisplayServerEnums::MAIN_WINDOW_ID];
		if (win.egl_surface != EGL_NO_SURFACE) {
			if (!eglMakeCurrent(display, win.egl_surface, win.egl_surface, context)) {
				check_egl_error("eglMakeCurrent (make_current)");
			}
		}
	}
}

void GLManager_Switch::release_current() {
	if (display != EGL_NO_DISPLAY) {
		if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
			check_egl_error("eglMakeCurrent (release_current)");
		}
	}
}

void GLManager_Switch::swap_buffers() {
	if (windows.has(DisplayServerEnums::MAIN_WINDOW_ID)) {
		GLWindow &win = windows[DisplayServerEnums::MAIN_WINDOW_ID];
		if (win.egl_surface != EGL_NO_SURFACE) {
			if (!eglSwapBuffers(display, win.egl_surface)) {
				check_egl_error("eglSwapBuffers");
			}
		}
	}

#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
	static uint64_t last_ms = 0;
	uint64_t now = nx::millis();
	if (last_ms != 0 && now - last_ms > 100) {
		TRACE("[GL_MANAGER] Slow frame %llu ms\n", (unsigned long long)(now - last_ms));
	}
	last_ms = now;
#endif
}

void GLManager_Switch::set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window_id) {
	(void)p_window_id;
	vsync_mode = p_vsync_mode;
	int interval = (p_vsync_mode == DisplayServerEnums::VSYNC_DISABLED) ? 0 : 1;
	if (!eglSwapInterval(display, interval)) {
		check_egl_error("eglSwapInterval");
	}
}

GLManager_Switch::GLManager_Switch() {
	// Does nothing.
}

GLManager_Switch::~GLManager_Switch() {
	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		for (KeyValue<DisplayServerEnums::WindowID, GLWindow> &E : windows) {
			if (E.value.egl_surface != EGL_NO_SURFACE) {
				eglDestroySurface(display, E.value.egl_surface);
				E.value.egl_surface = EGL_NO_SURFACE;
			}
		}
		windows.clear();

		if (context != EGL_NO_CONTEXT) {
			eglDestroyContext(display, context);
			context = EGL_NO_CONTEXT;
		}

		eglTerminate(display);
		display = EGL_NO_DISPLAY;
	}
}

#endif // GLES3_ENABLED
