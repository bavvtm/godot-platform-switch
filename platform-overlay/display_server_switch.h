#pragma once

#include "switch_wrap.h"

#include "core/input/input_event.h"
#include "core/math/vector2i.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "servers/display/display_server.h"
#include "servers/display/display_server_enums.h"

#ifdef GLES3_ENABLED
#include "gl_manager_switch.h"
#endif

#ifdef RD_ENABLED
class RenderingContextDriver;
class RenderingDevice;
#endif

class DisplayServerSwitch : public DisplayServer {
	GDSOFTCLASS(DisplayServerSwitch, DisplayServer);

private:
	struct WindowData {
		NWindow *nx_window = nullptr;
		ObjectID instance_id;
		Point2i size = Point2i(1280, 720);
		bool in_use = false;
		bool is_focused = true;

		Callable rect_changed_callback;
		Callable window_event_callback;
		Callable input_event_callback;
		Callable input_text_callback;
		Callable drop_files_callback;
	};

	WindowData main_window;
	String rendering_driver;
	DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
	int current_op_mode = -1;

	struct TouchPoint {
		bool active = false;
		Vector2 pos;
	};

	TouchPoint touch_points[nx::MAX_TOUCH_POINTS];
	Point2i last_mouse_position = Point2i(0, 0);

	Mutex inline_kbd_mutex;
	String inline_last_text;
	String inline_pending_text;
	bool inline_has_pending_text = false;
	bool inline_pending_submit = false;

#ifdef RD_ENABLED
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif

#ifdef GLES3_ENABLED
	GLManager_Switch *gl_manager = nullptr;
#endif

	static DisplayServer *create_func(
	    const String &p_rendering_driver,
	    DisplayServerEnums::WindowMode p_mode,
	    DisplayServerEnums::VSyncMode p_vsync_mode,
	    uint32_t p_flags,
	    const Vector2i *p_position,
	    const Vector2i &p_resolution,
	    int p_screen,
	    DisplayServerEnums::Context p_context,
	    int64_t p_parent_window,
	    Error &r_error);

	static Vector<String> get_rendering_drivers_func();

	static void _dispatch_input_events(const Ref<InputEvent> &p_event);
	static void _on_inline_keyboard_event(const char *p_text, bool p_submitted, bool p_canceled);

	void _process_inline_keyboard();
	void _check_operation_mode();

public:
	static DisplayServerSwitch *get_singleton();
	static void register_switch_driver();

	virtual bool has_feature(DisplayServerEnums::Feature p_feature) const override;
	virtual String get_name() const override;

	// touchscreen and events
	virtual bool is_touchscreen_available() const override { return true; }
	virtual void process_events() override;

	// mouse
	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;
	virtual void cursor_set_shape(DisplayServerEnums::CursorShape p_shape) override {}
	virtual DisplayServerEnums::CursorShape cursor_get_shape() const override { return DisplayServerEnums::CURSOR_ARROW; }
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, DisplayServerEnums::CursorShape p_shape = DisplayServerEnums::CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override {}

	// virtual keyboard
	virtual void virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2(), DisplayServerEnums::VirtualKeyboardType p_type = DisplayServerEnums::KEYBOARD_TYPE_DEFAULT, int p_max_length = -1, int p_cursor_start = -1, int p_cursor_end = -1) override;
	virtual void virtual_keyboard_hide() override;

	// rendering & vsync
	virtual void swap_buffers() override;
	virtual void window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return vsync_mode; }

#ifdef RD_ENABLED
	virtual RenderingDevice *window_get_active_rendering_device(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const {
		return rendering_device;
	}
	virtual RenderingContextDriver *window_get_active_rendering_context(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const {
		return rendering_context;
	}
#endif

	// screen queries
	virtual int get_screen_count() const override { return 1; }
	virtual int get_primary_screen() const override { return 0; }
	virtual Point2i screen_get_position(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return Point2i(0, 0); }
	virtual Size2i screen_get_size(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_scale(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return 1.0f; }
	virtual float screen_get_refresh_rate(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return 60.0f; }

	// window management
	virtual Vector<DisplayServerEnums::WindowID> get_window_list() const override {
		Vector<DisplayServerEnums::WindowID> list;
		if (main_window.in_use) {
			list.push_back(DisplayServerEnums::MAIN_WINDOW_ID);
		}
		return list;
	}

	virtual DisplayServerEnums::WindowID get_window_at_screen_position(const Point2i &p_position) const override { return DisplayServerEnums::MAIN_WINDOW_ID; }
	virtual int64_t window_get_native_handle(DisplayServerEnums::HandleType p_handle_type, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {
		if (p_window == DisplayServerEnums::MAIN_WINDOW_ID) {
			main_window.instance_id = p_instance;
		}
	}
	virtual ObjectID window_get_attached_instance_id(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override {
		return (p_window == DisplayServerEnums::MAIN_WINDOW_ID) ? main_window.instance_id : ObjectID();
	}

	virtual void window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	virtual void window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual int window_get_current_screen(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return 0; }
	virtual void window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}

	virtual Point2i window_get_position(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return Point2i(0, 0); }
	virtual Point2i window_get_position_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return Point2i(0, 0); }
	virtual void window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual void window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) override {}

	virtual void window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual Size2i window_get_max_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return main_window.size; }
	virtual void window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual Size2i window_get_min_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return main_window.size; }

	virtual Size2i window_get_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return window_get_size(p_window); }
	virtual void window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	virtual void window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual DisplayServerEnums::WindowMode window_get_mode(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return DisplayServerEnums::WINDOW_MODE_FULLSCREEN; }
	virtual bool window_is_maximize_allowed(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return false; }

	virtual void window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual bool window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return false; }

	virtual void window_request_attention(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}
	virtual void window_move_to_foreground(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override {}

	virtual bool window_is_focused(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return main_window.is_focused; }
	virtual bool window_can_draw(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return main_window.in_use; }
	virtual bool can_any_window_draw() const override { return main_window.in_use; }

	void dispatch_window_event(DisplayServerEnums::WindowEvent p_event);
	void send_input_event(const Ref<InputEvent> &p_event) const;

	DisplayServerSwitch(const String &p_rendering_driver, DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error);
	~DisplayServerSwitch();
};
