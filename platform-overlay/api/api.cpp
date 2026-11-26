#include "api.h"
#include "switch_api_singleton.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/templates/vector.h"

#ifdef __SWITCH__
#include "../switch_wrap.h"
#endif

static SwitchAPI *switch_api_singleton = nullptr;

void register_switch_api() {
	GDREGISTER_CLASS(SwitchAPI);
	switch_api_singleton = memnew(SwitchAPI);
	Engine::get_singleton()->add_singleton(Engine::Singleton("SwitchAPI", switch_api_singleton));
}

void unregister_switch_api() {
	if (switch_api_singleton) {
		Engine::get_singleton()->remove_singleton("SwitchAPI");
		memdelete(switch_api_singleton);
		switch_api_singleton = nullptr;
	}
}

SwitchAPI *SwitchAPI::singleton = nullptr;

void SwitchAPI::_bind_methods() {
	ClassDB::bind_method(
	    D_METHOD("show_keyboard", "header_text", "sub_text", "guide_text", "initial_text", "max_length", "preset"),
	    &SwitchAPI::show_keyboard,
	    DEFVAL(""),
	    DEFVAL(""),
	    DEFVAL(""),
	    DEFVAL(""),
	    DEFVAL(32),
	    DEFVAL(0));

	ClassDB::bind_method(D_METHOD("is_docked"), &SwitchAPI::is_docked);
	ClassDB::bind_method(D_METHOD("get_firmware_version"), &SwitchAPI::get_firmware_version);
}

String SwitchAPI::show_keyboard(const String &p_header, const String &p_sub, const String &p_guide, const String &p_initial, int p_max_len, int p_preset) {
#ifdef __SWITCH__
	SwkbdConfig kbd;
	Result rc = swkbdCreate(&kbd, 0);
	if (R_FAILED(rc)) {
		ERR_PRINT(vformat("SwitchAPI: swkbdCreate failed (0x%x)", rc));
		return p_initial;
	}

	switch (p_preset) {
		case 1: // Password
			swkbdConfigMakePresetPassword(&kbd);
			break;
		case 2: // UserName
			swkbdConfigMakePresetUserName(&kbd);
			break;
		case 3: // NumPad
			swkbdConfigMakePresetDefault(&kbd);
			swkbdConfigSetType(&kbd, SwkbdType_NumPad);
			break;
		default: // Default
			swkbdConfigMakePresetDefault(&kbd);
			break;
	}

	u32 max_len = (p_max_len > 0 && p_max_len <= 500) ? (u32)p_max_len : 500;
	swkbdConfigSetStringLenMax(&kbd, max_len);

	if (!p_header.is_empty()) {
		swkbdConfigSetHeaderText(&kbd, p_header.utf8().get_data());
	}
	if (!p_sub.is_empty()) {
		swkbdConfigSetSubText(&kbd, p_sub.utf8().get_data());
	}
	if (!p_guide.is_empty()) {
		swkbdConfigSetGuideText(&kbd, p_guide.utf8().get_data());
	}
	if (!p_initial.is_empty()) {
		swkbdConfigSetInitialText(&kbd, p_initial.utf8().get_data());
	}

	Vector<char> out_text;
	out_text.resize(2048);
	out_text.fill(0);

	rc = swkbdShow(&kbd, out_text.ptrw(), out_text.size());
	swkbdClose(&kbd);

	if (R_SUCCEEDED(rc)) {
		return String::utf8(out_text.ptr());
	}
#endif
	return p_initial;
}

bool SwitchAPI::is_docked() const {
#ifdef __SWITCH__
	return nx::is_docked();
#else
	return false;
#endif
}

String SwitchAPI::get_firmware_version() const {
#ifdef __SWITCH__
	Vector<char> buf;
	buf.resize(64);
	buf.fill(0);

	if (nx::firmware_version(buf.ptrw(), buf.size())) {
		return String::utf8(buf.ptr());
	}
#endif
	return String();
}

SwitchAPI::SwitchAPI() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "SwitchAPI singleton already exists.");
	singleton = this;
}
