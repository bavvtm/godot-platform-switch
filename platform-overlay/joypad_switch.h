#pragma once

#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/input/input.h"
#include "core/templates/hash_map.h"
#include "switch_wrap.h"

#define JOYPADS_MAX 8

class JoypadSwitch {
private:
	static inline HashMap<HidNpadButton, JoyButton> button_map;

	bool connected[JOYPADS_MAX] = {};
	u64 prev_buttons[JOYPADS_MAX] = {};
	PadState pads[JOYPADS_MAX];

	HidVibrationDeviceHandle vibration_handles[JOYPADS_MAX][2] = {};
	int vibration_handle_count[JOYPADS_MAX] = {};
	u32 active_pad_id[JOYPADS_MAX] = {};
	u32 active_pad_style[JOYPADS_MAX] = {};
	uint64_t last_vibration_timestamp[JOYPADS_MAX] = {};
	bool is_vibrating[JOYPADS_MAX] = {};

	Input *input = nullptr;

	void _init_pad();

	void _init_vibration(int p_index);
	void _stop_vibration(int p_index);
	void _send_vibration(int p_index, float p_low, float p_high);

public:
	Error initialize();
	void process_pad();
	void reset_pad_state();

	void process_vibration();
	void reset_vibration();

	JoypadSwitch();
	~JoypadSwitch();
};
