#include "editor_export_platform_switch.h"
#include "switch_utils.h"

#if __has_include("logo_data.gen.h")
#include "logo_data.gen.h"
#define HAS_GENERATED_LOGO 1
#endif

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/color.hpp>

#include <cstring>

TypedArray<Dictionary> EditorExportPlatformSwitch::_get_export_options() const {
	TypedArray<Dictionary> options;

	Dictionary opt_template_release;
	opt_template_release["name"] = "custom_template/release";
	opt_template_release["type"] = Variant::STRING;
	opt_template_release["hint"] = PROPERTY_HINT_FILE;
	opt_template_release["hint_string"] = "*";
	opt_template_release["default_value"] = String(SwitchUtils::TEMPLATE_DIR).path_join("release");
	options.push_back(opt_template_release);

	Dictionary opt_template_debug;
	opt_template_debug["name"] = "custom_template/debug";
	opt_template_debug["type"] = Variant::STRING;
	opt_template_debug["hint"] = PROPERTY_HINT_FILE;
	opt_template_debug["hint_string"] = "*";
	opt_template_debug["default_value"] = String(SwitchUtils::TEMPLATE_DIR).path_join("debug");
	options.push_back(opt_template_debug);

	Dictionary opt_debug;
	opt_debug["name"] = "binary_format/debug_mode";
	opt_debug["type"] = Variant::BOOL;
	opt_debug["default_value"] = false;
	options.push_back(opt_debug);

	Dictionary opt_embed;
	opt_embed["name"] = "binary_format/embed_pck";
	opt_embed["type"] = Variant::BOOL;
	opt_embed["default_value"] = false;
	options.push_back(opt_embed);

	Dictionary opt_icon;
	opt_icon["name"] = "application/icon";
	opt_icon["type"] = Variant::STRING;
	opt_icon["hint"] = PROPERTY_HINT_FILE;
	opt_icon["hint_string"] = "*.png,*.jpg,*.jpeg,*.webp,*.svg";
	opt_icon["default_value"] = get_project_setting("application/config/icon", String(SwitchUtils::DEFAULT_ICON));
	options.push_back(opt_icon);

	Dictionary opt_title;
	opt_title["name"] = "application/app_title";
	opt_title["type"] = Variant::STRING;
	opt_title["default_value"] = get_project_setting("application/config/name", String(SwitchUtils::DEFAULT_TITLE));
	options.push_back(opt_title);

	Dictionary opt_version;
	opt_version["name"] = "application/version";
	opt_version["type"] = Variant::STRING;
	opt_version["default_value"] = get_project_setting("application/config/version", String(SwitchUtils::DEFAULT_VERSION));
	options.push_back(opt_version);

	Dictionary opt_author;
	opt_author["name"] = "application/author";
	opt_author["type"] = Variant::STRING;
	opt_author["default_value"] = SwitchUtils::DEFAULT_AUTHOR;
	options.push_back(opt_author);

	return options;
}

String EditorExportPlatformSwitch::_get_export_option_warning(const Ref<EditorExportPreset> &p_preset, const StringName &p_option) const {
	bool debug_mode = p_preset->get("binary_format/debug_mode");

	if (p_option == StringName("custom_template/release")) {
		if (debug_mode) return "";
		String path = p_preset->get("custom_template/release");
		if (path.is_empty() || !FileAccess::file_exists(path)) {
			return "Release template ELF not found. Set a valid path.";
		}
	}

	if (p_option == StringName("custom_template/debug")) {
		if (!debug_mode) return "";
		String path = p_preset->get("custom_template/debug");
		if (path.is_empty() || !FileAccess::file_exists(path)) {
			return "Debug template ELF not found. Set a valid path.";
		}
	}

	if (p_option == StringName("application/icon")) {
		String icon = p_preset->get("application/icon");
		if (icon.is_empty()) {
			return "Icon is empty. Default icon will be used.";
		}
		if (!FileAccess::file_exists(icon)) {
			return "Icon file not found. Default icon will be used.";
		}
	}

	if (p_option == StringName("application/app_title")) {
		String title = p_preset->get("application/app_title");
		if (title.is_empty()) {
			return "App title is empty. Project name will be used.";
		}
	}

	if (p_option == StringName("application/version")) {
		String version = p_preset->get("application/version");
		if (version.is_empty()) {
			return "Version is empty. Default version will be used.";
		}
	}

	if (p_option == StringName("application/author")) {
		String author = p_preset->get("application/author");
		if (author.is_empty()) {
			return "Author is empty. Default author will be used.";
		}
	}

	return "";
}

PackedStringArray EditorExportPlatformSwitch::_get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	PackedStringArray extensions;
	extensions.push_back("nro");
	return extensions;
}

PackedStringArray EditorExportPlatformSwitch::_get_platform_features() const {
	PackedStringArray features;
	features.push_back("switch");
	features.push_back("console");
	features.push_back("arm64");
	return features;
}

PackedStringArray EditorExportPlatformSwitch::_get_preset_features(const Ref<EditorExportPreset> &p_preset) const {
	PackedStringArray features;
	features.push_back("switch");
	features.push_back("arm64");
	features.push_back("s3tc");
	return features;
}

bool EditorExportPlatformSwitch::_is_executable(const String &p_path) const {
	return p_path.get_extension().to_lower() == "nro";
}

bool EditorExportPlatformSwitch::_has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, bool p_debug) const {
	String err;
	bool valid = true;
	bool missing_templates = false;

	bool debug_mode = p_preset->get("binary_format/debug_mode");
	String template_key = debug_mode ? "custom_template/debug" : "custom_template/release";
	String template_path = p_preset->get(template_key);

	if (template_path.is_empty() || !FileAccess::file_exists(template_path)) {
		err += (debug_mode ? "Debug" : "Release") + String(" template ELF not found at: ") + template_path + "\n";
		valid = false;
		missing_templates = true;
	}

	if (SwitchUtils::find_dkp_tool("nacptool").is_empty()) {
		err += "'nacptool' not found in addon bin/ or DEVKITPRO.\n";
		valid = false;
	}

	if (SwitchUtils::find_dkp_tool("elf2nro").is_empty()) {
		err += "'elf2nro' not found in addon bin/ or DEVKITPRO.\n";
		valid = false;
	}

	String icon_path = p_preset->get("application/icon");
	if (!icon_path.is_empty() && !FileAccess::file_exists(icon_path)) {
		err += "Icon file not found at: " + icon_path + " (default icon will be used).\n";
	}

	set_config_missing_templates(missing_templates);
	set_config_error(err.strip_edges());

	return valid;
}

bool EditorExportPlatformSwitch::_can_export(const Ref<EditorExportPreset> &p_preset, bool p_debug) const {
	return _has_valid_export_configuration(p_preset, p_debug);
}

Error EditorExportPlatformSwitch::_pck_add_file(const String &p_path, const PackedByteArray &p_data,
    int32_t p_file_index, int32_t p_total_files,
    const PackedStringArray &p_enc_include_filters,
    const PackedStringArray &p_enc_exclude_filters,
    const PackedByteArray &p_key) {
	if (active_packer.is_null()) {
		return ERR_UNCONFIGURED;
	}

	if (p_path.begins_with(String(SwitchUtils::EXPORTER_DIR) + "/")) {
		return OK;
	}

	// filter switch_exporter.gdextension from extension_list.cfg.
	PackedByteArray final_data = p_data;
	if (p_path == "res://.godot/extension_list.cfg") {
		String cfg_text = String::utf8((const char *)p_data.ptr(), p_data.size());
		PackedStringArray lines = cfg_text.split("\n", false);
		PackedStringArray filtered;
		for (int i = 0; i < lines.size(); i++) {
			if (!lines[i].contains("switch_exporter.gdextension")) {
				filtered.push_back(lines[i]);
			}
		}

		if (filtered.is_empty()) {
			return OK;
		}

		final_data = (String("\n").join(filtered) + "\n").to_utf8_buffer();
	}

	String tmp_file = ProjectSettings::get_singleton()->globalize_path("res://.godot/switch_pck_entry.tmp");
	Ref<FileAccess> f = FileAccess::open(tmp_file, FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::push_error("Failed to open tmp file for PCK entry: ", tmp_file);
		return ERR_FILE_CANT_WRITE;
	}
	f->store_buffer(final_data);
	f.unref();

	Error err = active_packer->add_file(p_path, tmp_file);
	if (err != OK) {
		UtilityFunctions::push_error("Failed to add file to PCK: ", p_path, ": ", err);
	}

	SwitchUtils::remove_tmp_file(tmp_file);
	return err;
}

void EditorExportPlatformSwitch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_pck_add_file", "path", "data", "file_index", "total_files", "enc_include_filters", "enc_exclude_filters", "key"), &EditorExportPlatformSwitch::_pck_add_file);
}

Error EditorExportPlatformSwitch::_prepare_romfs(const String &p_src_romfs, const String &p_staging_romfs, const String &p_fused_pck) {
	DirAccess::make_dir_recursive_absolute(p_staging_romfs);

	String splash_src = p_src_romfs.path_join("applet_splash.rgba.z");
	if (FileAccess::file_exists(splash_src)) {
		Error err = DirAccess::copy_absolute(splash_src, p_staging_romfs.path_join("applet_splash.rgba.z"));
		if (err != OK) {
			UtilityFunctions::push_error("Failed to copy applet splash: ", splash_src);
			return err;
		}
	}

	if (!p_fused_pck.is_empty()) {
		Error err = DirAccess::copy_absolute(p_fused_pck, p_staging_romfs.path_join("game.pck"));
		if (err != OK) {
			UtilityFunctions::push_error("Failed to copy fused PCK to romfs: ", p_fused_pck);
			return err;
		}
	}

	return OK;
}

Error EditorExportPlatformSwitch::_generate_nacp_for_preset(const Ref<EditorExportPreset> &p_preset, const String &p_out_nacp) {
	String title = p_preset->get("application/app_title");
	if (title.is_empty()) title = SwitchUtils::DEFAULT_TITLE;

	String author = p_preset->get("application/author");
	if (author.is_empty()) author = SwitchUtils::DEFAULT_AUTHOR;

	String version = p_preset->get("application/version");
	if (version.is_empty()) version = SwitchUtils::DEFAULT_VERSION;

	Error err = SwitchUtils::generate_nacp(title, author, version, p_out_nacp);
	if (err != OK) {
		UtilityFunctions::push_error("Failed to generate NACP at ", p_out_nacp);
	}

	return err;
}

Error EditorExportPlatformSwitch::_convert_preset_icon(const Ref<EditorExportPreset> &p_preset, const String &p_out_jpg, bool &r_has_icon) {
	String icon_src = p_preset->get("application/icon");
	if (icon_src.is_empty() || !FileAccess::file_exists(icon_src)) {
		icon_src = SwitchUtils::DEFAULT_ICON;
	}

	r_has_icon = (SwitchUtils::convert_icon(icon_src, 256, p_out_jpg) == OK);
	if (!r_has_icon) {
		UtilityFunctions::push_warning("Failed to convert icon, NRO will be exported without icon.");
	}

	return OK;
}

Error EditorExportPlatformSwitch::_write_separate_pck(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_nro_path) {
	String pck_path = p_nro_path.get_basename() + ".pck";

	Error err = _build_pck(p_preset, p_debug, pck_path);
	if (err != OK) {
		UtilityFunctions::push_error("Failed to build separate PCK at ", pck_path);
		return err;
	}

	UtilityFunctions::print("Exported PCK: ", pck_path);
	return OK;
}

Error EditorExportPlatformSwitch::_build_pck(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_pck_path) {
	SwitchUtils::ensure_tmp_dir();

	Ref<PCKPacker> packer;
	packer.instantiate();

	Error start_err = packer->pck_start(p_pck_path);
	if (start_err != OK) {
		UtilityFunctions::push_error("Failed to start PCK packer at ", p_pck_path, ": ", start_err);
		return start_err;
	}

	active_packer = packer;

	Error export_err = export_project_files(p_preset, p_debug, Callable(this, "_pck_add_file"));
	Error flush_err = OK;
	if (export_err == OK) {
		flush_err = packer->flush(true);
		if (flush_err != OK) {
			UtilityFunctions::push_error("Failed to flush PCK at ", p_pck_path, ": ", flush_err);
		}
	} else {
		UtilityFunctions::push_error("Failed to export project files to PCK: ", export_err);
	}

	active_packer.unref();
	return (export_err != OK) ? export_err : flush_err;
}

Error EditorExportPlatformSwitch::_export_fail(Error p_err) {
	SwitchUtils::cleanup_switch_tmp();
	SwitchUtils::ensure_tmp_dir();
	return p_err;
}

Error EditorExportPlatformSwitch::_export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	String out_nro_abs = ps->globalize_path(p_path);
	String tmp_dir = ps->globalize_path(SwitchUtils::TMP_DIR);
	String romfs_dir = tmp_dir.path_join("romfs");
	String nacp_path = tmp_dir.path_join("control.nacp");
	String icon_jpg = tmp_dir.path_join("icon_converted.jpg");

	SwitchUtils::cleanup_switch_tmp();
	SwitchUtils::ensure_tmp_dir();

	bool debug_mode = p_preset->get("binary_format/debug_mode");
	bool embed_pck = p_preset->get("binary_format/embed_pck");
	String fused_pck;
	if (embed_pck) {
		fused_pck = tmp_dir.path_join("game.pck");
		Error err = _build_pck(p_preset, debug_mode, fused_pck);
		if (err != OK) return _export_fail(err);
	}

	Error err = _prepare_romfs(ps->globalize_path("res://romfs"), romfs_dir, fused_pck);
	if (err != OK) return _export_fail(err);

	err = _generate_nacp_for_preset(p_preset, nacp_path);
	if (err != OK) return _export_fail(err);

	bool has_icon = false;
	err = _convert_preset_icon(p_preset, icon_jpg, has_icon);
	if (err != OK) return _export_fail(err);

	String template_path = p_preset->get(debug_mode ? "custom_template/debug" : "custom_template/release");
	if (template_path.is_empty() || !FileAccess::file_exists(template_path)) {
		UtilityFunctions::push_error("Template ELF not found: ", template_path);
		return _export_fail(ERR_FILE_NOT_FOUND);
	}

	err = SwitchUtils::generate_nro(ps->globalize_path(template_path),
	    has_icon ? icon_jpg : "", nacp_path, romfs_dir, out_nro_abs);
	if (err != OK) {
		UtilityFunctions::push_error("Failed to generate NRO at ", out_nro_abs);
		return _export_fail(err);
	}

	if (!embed_pck) {
		err = _write_separate_pck(p_preset, debug_mode, out_nro_abs);
		if (err != OK) return _export_fail(err);
	}

	SwitchUtils::cleanup_switch_tmp();
	SwitchUtils::ensure_tmp_dir();
	return OK;
}

Error EditorExportPlatformSwitch::_export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	SwitchUtils::cleanup_switch_tmp();
	SwitchUtils::ensure_tmp_dir();

	bool debug_mode = p_preset->get("binary_format/debug_mode");
	Error err = _build_pck(p_preset, debug_mode, p_path);
	if (err != OK) {
		UtilityFunctions::push_error("Failed to export PCK at ", p_path, ": ", err);
		return _export_fail(err);
	}

	UtilityFunctions::print("Exported PCK: ", p_path);
	return OK;
}

void EditorExportPlatformSwitch::_initialize() {
	// Does nothing.
}

void EditorExportPlatformSwitch::_cleanup() {
	// Does nothing.
}

void EditorExportPlatformSwitch::initialize_logo() {
#ifdef HAS_GENERATED_LOGO
	PackedByteArray data;
	data.resize(sizeof(switch_logo_rgba));
	memcpy(data.ptrw(), switch_logo_rgba, sizeof(switch_logo_rgba));

	Ref<Image> img = Image::create_from_data(SWITCH_LOGO_SIZE, SWITCH_LOGO_SIZE, false, Image::FORMAT_RGBA8, data);
	logo = ImageTexture::create_from_image(img);

	Ref<Image> run_img = img->duplicate();
	run_img->resize(16, 16, Image::INTERPOLATE_LANCZOS);
	run_icon = ImageTexture::create_from_image(run_img);
#else
	Ref<Image> logo_img = Image::create_empty(32, 32, false, Image::FORMAT_RGBA8);
	logo_img->fill(Color(0.90f, 0.00f, 0.07f, 1.0f));
	logo = ImageTexture::create_from_image(logo_img);

	Ref<Image> run_img = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	run_img->fill(Color(0.90f, 0.00f, 0.07f, 1.0f));
	run_icon = ImageTexture::create_from_image(run_img);
#endif
}

EditorExportPlatformSwitch::EditorExportPlatformSwitch() {
	initialize_logo();
}

EditorExportPlatformSwitch::~EditorExportPlatformSwitch() {
	_cleanup();
}
