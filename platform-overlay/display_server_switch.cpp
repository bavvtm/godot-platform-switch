#include "display_server_switch.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/os.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#ifdef RD_ENABLED
#ifdef VULKAN_ENABLED
#include "rendering_context_driver_vulkan_switch.h"
#endif
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"
#endif

#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#include "gl_manager_switch.h"
#endif

DisplayServerSwitch *DisplayServerSwitch::get_singleton() {
	return static_cast<DisplayServerSwitch *>(DisplayServer::get_singleton());
}

bool DisplayServerSwitch::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
		case DisplayServerEnums::FEATURE_VIRTUAL_KEYBOARD:
			return true;
		default:
			return false;
	}
}

String DisplayServerSwitch::get_name() const {
	return "Switch";
}

void DisplayServerSwitch::_check_operation_mode() {
	AppletOperationMode op_mode = appletGetOperationMode();
	if ((int)op_mode != current_op_mode) {
		current_op_mode = (int)op_mode;

		// 0: Handheld (720p), 1: Console / Docked (1080p)
		Size2i new_size;
		if (op_mode == AppletOperationMode_Console) {
			new_size = Size2i(1920, 1080);
		} else {
			new_size = Size2i(1280, 720);
		}

		window_set_size(new_size, DisplayServerEnums::MAIN_WINDOW_ID);
	}
}

void DisplayServerSwitch::send_input_event(const Ref<InputEvent> &p_event) const {
	if (main_window.input_event_callback.is_valid()) {
		main_window.input_event_callback.call(p_event);
	}
}

void DisplayServerSwitch::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerSwitch *ds = DisplayServerSwitch::get_singleton();
	if (ds) {
		ds->send_input_event(p_event);
	}
}

void DisplayServerSwitch::dispatch_window_event(DisplayServerEnums::WindowEvent p_event) {
	if (p_event == DisplayServerEnums::WINDOW_EVENT_FOCUS_IN) {
		main_window.is_focused = true;
	} else if (p_event == DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT) {
		main_window.is_focused = false;
	}

	if (main_window.window_event_callback.is_valid()) {
		Variant ev = p_event;
		main_window.window_event_callback.call(ev);
	}
}

void DisplayServerSwitch::process_events() {
	HidTouchScreenState state = {};
	if (hidGetTouchScreenStates(&state, 1)) {
		const float scale_x = (float)main_window.size.x / 1280.0f;
		const float scale_y = (float)main_window.size.y / 720.0f;
		bool current_active[nx::MAX_TOUCH_POINTS] = {};

		for (s32 i = 0; i < state.count; i++) {
			const HidTouchState &t = state.touches[i];
			if (t.finger_id >= nx::MAX_TOUCH_POINTS) {
				continue;
			}

			current_active[t.finger_id] = true;
			Vector2 pos(t.x * scale_x, t.y * scale_y);
			if (t.finger_id == 0) {
				last_mouse_position = Point2i(pos.x, pos.y);
			}

			if (!touch_points[t.finger_id].active) {
				touch_points[t.finger_id].active = true;
				touch_points[t.finger_id].pos = pos;

				Ref<InputEventScreenTouch> ev;
				ev.instantiate();
				ev->set_index(t.finger_id);
				ev->set_position(pos);
				ev->set_pressed(true);
				Input::get_singleton()->parse_input_event(ev);
			} else if (touch_points[t.finger_id].pos != pos) {
				Vector2 rel = pos - touch_points[t.finger_id].pos;
				touch_points[t.finger_id].pos = pos;

				Ref<InputEventScreenDrag> ev;
				ev.instantiate();
				ev->set_index(t.finger_id);
				ev->set_position(pos);
				ev->set_relative(rel);
				Input::get_singleton()->parse_input_event(ev);
			}
		}

		for (u32 id = 0; id < nx::MAX_TOUCH_POINTS; id++) {
			if (touch_points[id].active && !current_active[id]) {
				touch_points[id].active = false;

				Ref<InputEventScreenTouch> ev;
				ev.instantiate();
				ev->set_index(id);
				ev->set_position(touch_points[id].pos);
				ev->set_pressed(false);
				Input::get_singleton()->parse_input_event(ev);
			}
		}
	}

	Input::get_singleton()->flush_buffered_events();

	_process_virtual_keyboard();
	_check_operation_mode();
}

Point2i DisplayServerSwitch::mouse_get_position() const {
	return last_mouse_position;
}

BitField<MouseButtonMask> DisplayServerSwitch::mouse_get_button_state() const {
	return touch_points[0].active ? MouseButtonMask::LEFT : MouseButtonMask::NONE;
}

void DisplayServerSwitch::_process_virtual_keyboard() {
	if (has_pending_keyboard_text) {
		has_pending_keyboard_text = false;
		if (main_window.input_text_callback.is_valid()) {
			main_window.input_text_callback.call(pending_keyboard_text);
		}
		pending_keyboard_text = String();
		return;
	}

	if (!keyboard_requested) {
		return;
	}

	if (keyboard_wait_frames > 0) {
		keyboard_wait_frames--;
		return;
	}

	keyboard_requested = false;

	SwkbdConfig kbd;
	Result rc = swkbdCreate(&kbd, 0);
	if (R_FAILED(rc)) {
		ERR_PRINT(vformat("Switch Swkbd: swkbdCreate failed (0x%x)", rc));
		return;
	}

	swkbdConfigMakePresetDefault(&kbd);
	swkbdConfigSetStringLenMax(&kbd, (u32)keyboard_max_length);

	if (!keyboard_existing_text.is_empty()) {
		swkbdConfigSetInitialText(&kbd, keyboard_existing_text.utf8().get_data());
	}

	char out_text[2048] = {};
	rc = swkbdShow(&kbd, out_text, sizeof(out_text));
	swkbdClose(&kbd);

	if (R_SUCCEEDED(rc)) {
		pending_keyboard_text = String::utf8(out_text);
		has_pending_keyboard_text = true;
	}
}

void DisplayServerSwitch::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, DisplayServerEnums::VirtualKeyboardType p_type, int p_max_length, int p_cursor_start, int p_cursor_end) {
	keyboard_requested = true;
	keyboard_wait_frames = KEYBOARD_DELAY_FRAMES;
	keyboard_existing_text = p_existing_text;
	keyboard_max_length = (p_max_length > 0 && p_max_length <= 500) ? p_max_length : 500;
}

void DisplayServerSwitch::virtual_keyboard_hide() {
	keyboard_requested = false;
}

DisplayServer *DisplayServerSwitch::create_func(
    const String &p_rendering_driver,
    DisplayServerEnums::WindowMode p_mode,
    DisplayServerEnums::VSyncMode p_vsync_mode,
    uint32_t p_flags,
    const Vector2i *p_position,
    const Vector2i &p_resolution,
    int p_screen,
    DisplayServerEnums::Context p_context,
    int64_t p_parent_window,
    Error &r_error) {
	(void)p_mode;
	(void)p_flags;
	(void)p_position;
	(void)p_resolution;
	(void)p_screen;
	(void)p_context;
	(void)p_parent_window;
	return memnew(DisplayServerSwitch(p_rendering_driver, p_vsync_mode, r_error));
}

Vector<String> DisplayServerSwitch::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
#endif
	return drivers;
}

void DisplayServerSwitch::register_switch_driver() {
	register_create_function("switch", create_func, get_rendering_drivers_func);
}

void DisplayServerSwitch::swap_buffers() {
#ifdef GLES3_ENABLED
	if (gl_manager) {
		gl_manager->swap_buffers();
	}
#endif
}

void DisplayServerSwitch::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	if (p_window != DisplayServerEnums::MAIN_WINDOW_ID) {
		return;
	}

	vsync_mode = p_vsync_mode;

#ifdef GLES3_ENABLED
	if (gl_manager) {
		gl_manager->set_vsync_mode(p_vsync_mode, p_window);
	}
#endif
}

Size2i DisplayServerSwitch::screen_get_size(int p_screen) const {
	(void)p_screen;
	return main_window.size;
}

int DisplayServerSwitch::screen_get_dpi(int p_screen) const {
	(void)p_screen;
	if (current_op_mode == AppletOperationMode_Console) {
		return 96; // TV or Monitor
	}

	if (nx::is_oled_model()) {
		return 210; // OLED
	} else if (nx::is_lite_model()) {
		return 267; // Lite
	}
	return 237; // V1/V2
}

Rect2i DisplayServerSwitch::screen_get_usable_rect(int p_screen) const {
	(void)p_screen;
	return Rect2i(Point2i(0, 0), main_window.size);
}

int64_t DisplayServerSwitch::window_get_native_handle(DisplayServerEnums::HandleType p_handle_type, DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V(p_window != DisplayServerEnums::MAIN_WINDOW_ID, 0);

	switch (p_handle_type) {
		case DisplayServerEnums::WINDOW_HANDLE:
			return reinterpret_cast<int64_t>(main_window.nx_window);
		case DisplayServerEnums::DISPLAY_HANDLE:
#ifdef GLES3_ENABLED
			if (gl_manager) {
				return reinterpret_cast<int64_t>(gl_manager->get_display());
			}
#endif
			return 0;
		case DisplayServerEnums::OPENGL_CONTEXT:
#ifdef GLES3_ENABLED
			if (gl_manager) {
				return reinterpret_cast<int64_t>(gl_manager->get_context());
			}
#endif
			return 0;
		default:
			return 0;
	}
}

void DisplayServerSwitch::window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	main_window.rect_changed_callback = p_callable;
}

void DisplayServerSwitch::window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	main_window.window_event_callback = p_callable;
}

void DisplayServerSwitch::window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	main_window.input_event_callback = p_callable;
}

void DisplayServerSwitch::window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	main_window.input_text_callback = p_callable;
}

void DisplayServerSwitch::window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	main_window.drop_files_callback = p_callable;
}

Point2i DisplayServerSwitch::window_get_size(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V(p_window != DisplayServerEnums::MAIN_WINDOW_ID, Point2i());
	return main_window.size;
}

void DisplayServerSwitch::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);

	if (main_window.size == p_size) {
		return;
	}

	main_window.size = p_size;

	if (main_window.nx_window) {
		nwindowSetDimensions(main_window.nx_window, p_size.x, p_size.y);
	}

#ifdef RD_ENABLED
	if (rendering_context) {
		rendering_context->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, p_size.x, p_size.y);
	}
#endif

#ifdef GLES3_ENABLED
	if (gl_manager) {
		gl_manager->window_resize(DisplayServerEnums::MAIN_WINDOW_ID, p_size);
	}
#endif

	if (main_window.rect_changed_callback.is_valid()) {
		main_window.rect_changed_callback.call(Rect2i(Point2i(0, 0), p_size));
	}
}

DisplayServerSwitch::DisplayServerSwitch(const String &p_rendering_driver, DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error) {
	rendering_driver = p_rendering_driver;
	vsync_mode = p_vsync_mode;

	NWindow *native_window = nwindowGetDefault();
	ERR_FAIL_NULL(native_window);

	main_window.nx_window = native_window;

	AppletOperationMode op_mode = appletGetOperationMode();
	current_op_mode = (int)op_mode;
	if (op_mode == AppletOperationMode_Console) {
		nwindowSetDimensions(native_window, 1920, 1080);
		main_window.size = Point2i(1920, 1080);
	} else {
		nwindowSetDimensions(native_window, 1280, 720);
		main_window.size = Point2i(1280, 720);
	}

	if (rendering_driver == "dummy") {
		RasterizerDummy::make_current();
	}

#ifdef RD_ENABLED
#ifdef VULKAN_ENABLED
	if (rendering_driver == "vulkan") {
		rendering_context = memnew(RenderingContextDriverVulkanSwitch);
	}
#endif // VULKAN_ENABLED
	if (rendering_context) {
		if (rendering_context->initialize() != OK) {
			memdelete(rendering_context);
			rendering_context = nullptr;
#ifdef GLES3_ENABLED
			bool fallback_to_opengl3 = GLOBAL_GET("rendering/rendering_device/fallback_to_opengl3");
			if (fallback_to_opengl3 && rendering_driver != "opengl3") {
				TRACE("Vulkan failed! switching to OpenGL 3");
				rendering_driver = "opengl3";
				OS::get_singleton()->set_current_rendering_method("gl_compatibility", OS::RENDERING_SOURCE_FALLBACK);
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver, OS::RENDERING_SOURCE_FALLBACK);
			} else
#endif
			{
				r_error = ERR_CANT_CREATE;
				if (p_rendering_driver == "vulkan") {
					OS::get_singleton()->alert("Unable to initialize Vulkan driver");
				}
				return;
			}
		} else {
#ifdef VULKAN_ENABLED
			RenderingContextDriverVulkanSwitch::WindowPlatformData wpd;
			wpd.window = main_window.nx_window;

			if (rendering_context->window_create(DisplayServerEnums::MAIN_WINDOW_ID, &wpd) != OK) {
				memdelete(rendering_context);
				rendering_context = nullptr;
				r_error = ERR_CANT_CREATE;
				return;
			}
#endif // VULKAN_ENABLED
		}
	}
#endif // RD_ENABLED

#ifdef GLES3_ENABLED
	if (rendering_driver == "opengl3") {
		gl_manager = memnew(GLManager_Switch);
		if (gl_manager->initialize() != OK) {
			memdelete(gl_manager);
			gl_manager = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}

		if (gl_manager->window_create(DisplayServerEnums::MAIN_WINDOW_ID, main_window.nx_window, main_window.size) != OK) {
			memdelete(gl_manager);
			gl_manager = nullptr;
			r_error = ERR_CANT_CREATE;
			return;
		}

		gl_manager->set_vsync_mode(vsync_mode, DisplayServerEnums::MAIN_WINDOW_ID);
		RasterizerGLES3::make_current(false);
	}
#endif

#ifdef RD_ENABLED
	if (rendering_context) {
		rendering_device = memnew(RenderingDevice);
		if (rendering_device->initialize(rendering_context, DisplayServerEnums::MAIN_WINDOW_ID) != OK) {
			memdelete(rendering_device);
			rendering_device = nullptr;
			rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
		rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
		RendererCompositorRD::make_current();
	}
#endif // RD_ENABLED

	main_window.in_use = true;
	main_window.is_focused = true;

	hidInitializeTouchScreen();

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	r_error = OK;
}

DisplayServerSwitch::~DisplayServerSwitch() {
	if (main_window.in_use) {
#ifdef RD_ENABLED
		if (rendering_device) {
			rendering_device->screen_free(DisplayServerEnums::MAIN_WINDOW_ID);
		}
		if (rendering_context) {
			rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		}
#endif
#ifdef GLES3_ENABLED
		if (gl_manager) {
			gl_manager->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		}
#endif
		main_window.in_use = false;
	}

#ifdef RD_ENABLED
	if (rendering_device) {
		memdelete(rendering_device);
		rendering_device = nullptr;
	}

	if (rendering_context) {
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif

#ifdef GLES3_ENABLED
	if (gl_manager) {
		memdelete(gl_manager);
		gl_manager = nullptr;
	}
#endif
}
