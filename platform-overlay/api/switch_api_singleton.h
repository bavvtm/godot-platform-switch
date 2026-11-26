#pragma once

#include "core/object/object.h"
#include "core/string/ustring.h"

class SwitchAPI : public Object {
	GDCLASS(SwitchAPI, Object);

	static SwitchAPI *singleton;

protected:
	static void _bind_methods();

public:
	static SwitchAPI *get_singleton() { return singleton; }

	String show_keyboard(
	    const String &p_header, const String &p_sub,
	    const String &p_guide, const String &p_initial,
	    int p_max_len, int p_preset);

	bool is_docked() const;
	String get_firmware_version() const;

	SwitchAPI();
	~SwitchAPI() { singleton = nullptr; }
};
