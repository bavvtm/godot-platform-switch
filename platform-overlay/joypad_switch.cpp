#include "joypad_switch.h"

static constexpr u64 pad_ids[JOYPADS_MAX] = {
	(1ull << HidNpadIdType_No1) | (1ull << HidNpadIdType_Handheld),
	(1ull << HidNpadIdType_No2),
	(1ull << HidNpadIdType_No3),
	(1ull << HidNpadIdType_No4),
	(1ull << HidNpadIdType_No5),
	(1ull << HidNpadIdType_No6),
	(1ull << HidNpadIdType_No7),
	(1ull << HidNpadIdType_No8),
};

void JoypadSwitch::_init_pad() {
	// Switch Joypad to Godot JoyButton map
	button_map[HidNpadButton_B] = JoyButton::A;
	button_map[HidNpadButton_A] = JoyButton::B;
	button_map[HidNpadButton_Y] = JoyButton::X;
	button_map[HidNpadButton_X] = JoyButton::Y;
	button_map[HidNpadButton_L] = JoyButton::LEFT_SHOULDER;
	button_map[HidNpadButton_R] = JoyButton::RIGHT_SHOULDER;
	button_map[HidNpadButton_StickL] = JoyButton::LEFT_STICK;
	button_map[HidNpadButton_StickR] = JoyButton::RIGHT_STICK;
	button_map[HidNpadButton_Minus] = JoyButton::BACK;
	button_map[HidNpadButton_Plus] = JoyButton::START;
	button_map[HidNpadButton_Up] = JoyButton::DPAD_UP;
	button_map[HidNpadButton_Down] = JoyButton::DPAD_DOWN;
	button_map[HidNpadButton_Left] = JoyButton::DPAD_LEFT;
	button_map[HidNpadButton_Right] = JoyButton::DPAD_RIGHT;
}

Error JoypadSwitch::initialize() {
	input = Input::get_singleton();
	ERR_FAIL_NULL_V(input, ERR_UNCONFIGURED);

	if (button_map.is_empty()) {
		_init_pad();
	}

	padConfigureInput(JOYPADS_MAX, HidNpadStyleSet_NpadStandard);

	for (int i = 0; i < JOYPADS_MAX; i++) {
		padInitializeWithMask(&pads[i], pad_ids[i]);
		connected[i] = false;
	}

	return OK;
}

void JoypadSwitch::process_pad() {
	if (!input) {
		return;
	}

	for (int index = 0; index < JOYPADS_MAX; index++) {
		padUpdate(&pads[index]);

		bool is_connected = padIsConnected(&pads[index]);
		if (is_connected != connected[index]) {
			connected[index] = is_connected;
			prev_buttons[index] = 0;
			if (is_connected) {
				input->joy_connection_changed(index, true, "Nintendo Switch Controller", "__SWITCH_GAMEPAD__");
			} else {
				input->joy_connection_changed(index, false, "");
			}
		}

		if (!is_connected) {
			continue;
		}

		// analog sticks
		HidAnalogStickState l_stick = padGetStickPos(&pads[index], 0);
		HidAnalogStickState r_stick = padGetStickPos(&pads[index], 1);

		input->joy_axis(index, JoyAxis::LEFT_X, CLAMP(l_stick.x / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(index, JoyAxis::LEFT_Y, CLAMP(-l_stick.y / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(index, JoyAxis::RIGHT_X, CLAMP(r_stick.x / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(index, JoyAxis::RIGHT_Y, CLAMP(-r_stick.y / 32767.0f, -1.0f, 1.0f));

		u64 held = padGetButtons(&pads[index]);
		u64 changed = held ^ prev_buttons[index];
		prev_buttons[index] = held;

		if (changed != 0) {
			for (const KeyValue<HidNpadButton, JoyButton> &E : button_map) {
				if (changed & E.key) {
					bool is_pressed = (held & E.key) != 0;
					input->joy_button(index, E.value, is_pressed);
				}
			}

			// ZL / ZR trigger axes
			if (changed & HidNpadButton_ZL) {
				input->joy_axis(index, JoyAxis::TRIGGER_LEFT, (held & HidNpadButton_ZL) ? 1.0f : 0.0f);
			}
			if (changed & HidNpadButton_ZR) {
				input->joy_axis(index, JoyAxis::TRIGGER_RIGHT, (held & HidNpadButton_ZR) ? 1.0f : 0.0f);
			}
		}
	}
}

void JoypadSwitch::reset_pad_state() {
	if (!input) {
		return;
	}

	for (int index = 0; index < JOYPADS_MAX; index++) {
		if (!connected[index]) {
			prev_buttons[index] = 0;
			continue;
		}

		if (prev_buttons[index] != 0) {
			for (const KeyValue<HidNpadButton, JoyButton> &E : button_map) {
				if (prev_buttons[index] & E.key) {
					input->joy_button(index, E.value, false);
				}
			}

			// zl zr
			if (prev_buttons[index] & HidNpadButton_ZL) {
				input->joy_axis(index, JoyAxis::TRIGGER_LEFT, 0.0f);
			}
			if (prev_buttons[index] & HidNpadButton_ZR) {
				input->joy_axis(index, JoyAxis::TRIGGER_RIGHT, 0.0f);
			}

			prev_buttons[index] = 0;
		}

		// centered analog
		input->joy_axis(index, JoyAxis::LEFT_X, 0.0f);
		input->joy_axis(index, JoyAxis::LEFT_Y, 0.0f);
		input->joy_axis(index, JoyAxis::RIGHT_X, 0.0f);
		input->joy_axis(index, JoyAxis::RIGHT_Y, 0.0f);
	}
}

JoypadSwitch::JoypadSwitch() {
	// Does nothing.
}

JoypadSwitch::~JoypadSwitch() {
	if (!input) {
		return;
	}

	for (int i = 0; i < JOYPADS_MAX; i++) {
		if (connected[i]) {
			input->joy_connection_changed(i, false, "");
			connected[i] = false;
		}
	}
}
