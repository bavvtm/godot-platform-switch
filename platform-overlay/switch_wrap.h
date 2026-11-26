#pragma once

#include <cstddef>
#include <cstdint> // Wajib di luar #ifdef __SWITCH__

#ifdef __SWITCH__
#define Mutex LibnxMutex
#define Semaphore LibnxSemaphore
#define Thread LibnxThread
#define AudioDriver LibnxAudioDriver

#include <switch.h>
#undef Thread
#undef Mutex
#undef Semaphore
#undef AudioDriver
#endif

#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
#define TRACE(fmt, ...) printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) ((void)0)
#endif

namespace nx {

constexpr uint32_t MAX_TOUCH_POINTS = 16;

enum HardwareModel : uint32_t {
	HARDWARE_ICOSA = 0,  // V1 (Erista)
	HARDWARE_COPPER = 1, // Erista DevKit
	HARDWARE_HOAG = 2,   // Switch Lite
	HARDWARE_IOWA = 3,   // V2 (Mariko)
	HARDWARE_CALCIO = 4, // Mariko DevKit
	HARDWARE_AULA = 5,   // Switch OLED
};

bool firmware_version(char *r_buf, size_t p_len);
uint32_t hardware_type();

bool is_oled_model();
bool is_lite_model();

bool is_docked(); // Tanpa const

void ensure_dir(const char *p_path);
void init_file_logging_from_argv(const char *p_argv0);

int load_args_from_file(const char *p_argv0, char ***r_out_argv);
int build_setup_args(int argc, char *argv[], char ***r_out_argv);

uint64_t millis(); // Tanpa inline

// virtual keyboard
enum KeyboardPreset : int {
	KEYBOARD_PRESET_DEFAULT = 0,
	KEYBOARD_PRESET_PASSWORD = 1,
	KEYBOARD_PRESET_USERNAME = 2,
	KEYBOARD_PRESET_NUMPAD = 3,
};

using InlineKeyboardCb = void (*)(const char *p_text, bool p_submitted, bool p_canceled);

bool init_inline_keyboard(InlineKeyboardCb p_callback);
void shutdown_inline_keyboard();
void show_inline_keyboard(const char *p_initial_text, uint32_t p_max_length, KeyboardPreset p_preset);
void hide_inline_keyboard();
void update_inline_keyboard();

} // namespace nx
