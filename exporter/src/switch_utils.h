#pragma once

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

namespace SwitchUtils {

// path & default values
inline constexpr const char *EXPORTER_DIR = "res://addons/switch_exporter";
inline constexpr const char *BIN_DIR = "res://addons/switch_exporter/bin";
inline constexpr const char *TEMPLATE_DIR = "res://addons/switch_exporter/templates";
inline constexpr const char *TMP_DIR = "res://.godot/switch_export_tmp";

inline constexpr const char *DEFAULT_TITLE = "Godot Project";
inline constexpr const char *DEFAULT_VERSION = "1.0.0";
inline constexpr const char *DEFAULT_AUTHOR = "Homebrew Developer";
inline constexpr const char *DEFAULT_ICON = "res://addons/switch_exporter/default_icon.png";

// tool wrapper
String find_dkp_tool(const String &p_tool_name);

Error generate_nacp(const String &p_title, const String &p_author,
    const String &p_version, const String &p_out_path);

Error generate_nro(const String &p_template_elf, const String &p_icon_path,
    const String &p_nacp_path, const String &p_romfs_dir, const String &p_out_path);

// utils
Error convert_icon(const String &p_src_icon_path, int p_size, const String &p_out_jpg_path);

void remove_tmp_file(const String &p_abs_path);
void remove_tmp_dir(const String &p_abs_path);

void ensure_tmp_dir();
void cleanup_switch_tmp();

} // namespace SwitchUtils
