#include "os_switch.h"
#include "switch_wrap.h"

#include "core/io/dir_access.h"
#include "core/os/main_loop.h"
#include "main/main.h"
#include "display_server_switch.h"

#include <cstring>

void OS_Switch::alert(const String &p_alert, const String &p_title) {
	ErrorApplicationConfig config;
	errorApplicationCreate(&config, p_title.utf8().ptr(), p_alert.utf8().ptr());
	errorApplicationShow(&config);
}

void OS_Switch::initialize() {
	OS_Unix::initialize_core();
}

void OS_Switch::initialize_joypads() {
	if (joypad_switch) {
		return;
	}

	joypad_switch = memnew(JoypadSwitch());
	if (joypad_switch->initialize() != OK) {
		ERR_PRINT("Couldn't initialize Switch joypad input driver.");
		memdelete(joypad_switch);
		joypad_switch = nullptr;
	}
}

String OS_Switch::get_identifier() const {
	return "nintendo_switch";
}

String OS_Switch::get_name() const {
	return "Nintendo Switch";
}

String OS_Switch::get_distribution_name() const {
	return "HorizonOS";
}

String OS_Switch::get_model_name() const {
	u32 type = nx::hardware_type();
	switch (type) {
		case nx::HARDWARE_AULA:
			return "OLED";
		case nx::HARDWARE_HOAG:
			return "Lite";
		case nx::HARDWARE_ICOSA:
		case nx::HARDWARE_IOWA:
			return "V1/V2";
		case nx::HARDWARE_CALCIO:
			return "SDEV";
		default:
			return "Generic";
	}
}

String OS_Switch::get_version() const {
	char buf[32];
	return nx::firmware_version(buf, sizeof(buf)) ? String(buf) : String("unknown");
}

String OS_Switch::get_unique_id() const {
	return "nx";
}

String OS_Switch::get_processor_name() const {
	u32 type = nx::hardware_type();
	if (type == 0xFFFFFFFF) {
		return "Generic";
	}

	if (type == nx::HARDWARE_ICOSA || type == nx::HARDWARE_COPPER) {
		return "Erista";
	} else {
		return "Mariko";
	}
}

String OS_Switch::get_app_data_dir() const {
	String base_dir = get_executable_path().get_base_dir();
	if (base_dir.is_empty() || base_dir == ".") {
		return "sdmc:/switch";
	}
	return base_dir;
}

String OS_Switch::get_executable_path() const {
	return s_executable_path;
}

void OS_Switch::set_executable_path(const char *p_execpath) {
	s_executable_path = p_execpath;
}

String OS_Switch::get_data_path() const {
	return get_app_data_dir().path_join("data");
}

String OS_Switch::get_config_path() const {
	return get_app_data_dir().path_join("config");
}

String OS_Switch::get_cache_path() const {
	return get_app_data_dir().path_join("cache");
}

String OS_Switch::get_user_data_dir() const {
	static String user_dir;
	if (user_dir.is_empty()) {
		user_dir = get_data_path();
		nx::ensure_dir(user_dir.utf8().get_data());
	}
	return user_dir;
}

Error OS_Switch::get_entropy(uint8_t *r_buffer, int p_bytes) {
	if (p_bytes <= 0) {
		return OK;
	}
	if (R_FAILED(csrngGetRandomBytes(r_buffer, p_bytes))) {
		return FAILED;
	}
	return OK;
}

MainLoop *OS_Switch::get_main_loop() const {
	return main_loop;
}

void OS_Switch::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_Switch::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_Switch::run() {
	if (!main_loop) {
		return;
	}

	main_loop->initialize();

#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
	bool first_frame = true;
#endif
	bool last_in_focus = true;
	while (appletMainLoop()) {
		bool in_focus = (appletGetFocusState() == AppletFocusState_InFocus);
		if (in_focus != last_in_focus) {
			last_in_focus = in_focus;

			audio_driver.set_pause(!in_focus);

			if (DisplayServerSwitch::get_singleton()) {
				DisplayServerSwitch::get_singleton()->dispatch_window_event(
				    in_focus ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN : DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT);
			}
		}

		if (joypad_switch) {
			if (in_focus) {
				joypad_switch->process_pad();
				joypad_switch->process_vibration();
			} else {
				joypad_switch->reset_pad_state();
				joypad_switch->reset_vibration();
			}
		}

		DisplayServer::get_singleton()->process_events();
		if (Main::iteration()) {
			break;
		}

#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
		if (first_frame) {
			first_frame = false;
			TRACE("[BOOT] %llu ms: First frame rendered!", (unsigned long long)nx::millis());
		}
#endif
	}

	main_loop->finalize();
}

void OS_Switch::finalize() {
	if (joypad_switch) {
		memdelete(joypad_switch);
		joypad_switch = nullptr;
	}

	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;

	OS_Unix::finalize_core();
}

OS_Switch::OS_Switch() {
	main_loop = nullptr;

	AudioDriverManager::add_driver(&audio_driver);

	DisplayServerSwitch::register_switch_driver();
}

OS_Switch::~OS_Switch() {
	finalize();
}
