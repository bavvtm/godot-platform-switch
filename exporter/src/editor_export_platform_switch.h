#pragma once

#include <godot_cpp/classes/editor_export_platform.hpp>
#include <godot_cpp/classes/editor_export_platform_extension.hpp>
#include <godot_cpp/classes/editor_export_preset.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/pck_packer.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

class EditorExportPlatformSwitch : public EditorExportPlatformExtension {
	GDCLASS(EditorExportPlatformSwitch, EditorExportPlatformExtension);

private:
	Ref<PCKPacker> active_packer;

	Ref<ImageTexture> logo;
	Ref<ImageTexture> run_icon;

	void initialize_logo();

	String get_project_setting(const String &p_key, const String &p_fallback) const {
		String v = ProjectSettings::get_singleton()->get_setting(p_key, "");
		return v.is_empty() ? p_fallback : v;
	}

	Error _prepare_romfs(const String &p_src_romfs, const String &p_staging_romfs, const String &p_fused_pck);
	Error _generate_nacp_for_preset(const Ref<EditorExportPreset> &p_preset, const String &p_out_nacp);
	Error _convert_preset_icon(const Ref<EditorExportPreset> &p_preset, const String &p_out_jpg, bool &r_has_icon);
	Error _write_separate_pck(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_nro_path);
	Error _build_pck(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_pck_path);
	Error _export_fail(Error p_err);

	Error _pck_add_file(const String &p_path, const PackedByteArray &p_data,
	    int32_t p_file_index, int32_t p_total_files,
	    const PackedStringArray &p_enc_include_filters,
	    const PackedStringArray &p_enc_exclude_filters,
	    const PackedByteArray &p_key);

protected:
	static void _bind_methods();

public:
	virtual String _get_name() const override { return "Nintendo Switch"; }
	virtual String _get_os_name() const override { return "Switch"; }
	virtual Ref<Texture2D> _get_logo() const override { return logo; }
	virtual Ref<Texture2D> _get_run_icon() const override { return run_icon; }

	virtual TypedArray<Dictionary> _get_export_options() const override;
	virtual String _get_export_option_warning(const Ref<EditorExportPreset> &p_preset, const StringName &p_option) const override;
	virtual bool _should_update_export_options() override { return false; }

	virtual PackedStringArray _get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual PackedStringArray _get_platform_features() const override;
	virtual PackedStringArray _get_preset_features(const Ref<EditorExportPreset> &p_preset) const override;

	virtual bool _has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, bool p_debug) const override;
	virtual bool _has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset) const override { return true; }

	virtual Error _export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) override;
	virtual Error _export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) override;

	virtual bool _is_executable(const String &p_path) const override;
	virtual bool _can_export(const Ref<EditorExportPreset> &p_preset, bool p_debug) const override;
	virtual void _initialize() override;
	virtual void _cleanup() override;

	EditorExportPlatformSwitch();
	~EditorExportPlatformSwitch() override;
};
