#include "switch_utils.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/color.hpp>

namespace SwitchUtils {

String find_dkp_tool(const String &p_tool_name) {
	OS *os = OS::get_singleton();
	bool is_win = os->get_name() == "Windows";
	String exe = is_win ? ".exe" : "";
	String tool = p_tool_name + exe;

	String bundled = ProjectSettings::get_singleton()->globalize_path(
	    String(BIN_DIR).path_join(is_win ? "windows" : "linux").path_join(tool));
	if (FileAccess::file_exists(bundled)) {
		return bundled;
	}

	String dkp = os->get_environment("DEVKITPRO");
	if (dkp.is_empty() && !is_win) {
		dkp = "/opt/devkitpro";
	}
	if (dkp.is_empty()) {
		return "";
	}

	const char *subdirs[] = { "bin", "tools/bin", "devkitA64/bin" };
	for (const char *sub : subdirs) {
		String path = dkp.path_join(String(sub).path_join(tool));
		if (FileAccess::file_exists(path)) {
			return path;
		}
	}

	return "";
}

Error generate_nacp(const String &p_title, const String &p_author,
    const String &p_version, const String &p_out_path) {
	String nacptool = find_dkp_tool("nacptool");
	if (nacptool.is_empty()) {
		return ERR_UNCONFIGURED;
	}

	PackedStringArray args;
	args.push_back("--create");
	args.push_back(p_title);
	args.push_back(p_author);
	args.push_back(p_version);
	args.push_back(p_out_path);

	Array output;
	int exit_code = OS::get_singleton()->execute(nacptool, args, output, true);
	if (exit_code != 0) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error generate_nro(const String &p_template_elf, const String &p_icon_path,
    const String &p_nacp_path, const String &p_romfs_dir,
    const String &p_out_path) {
	String elf2nro = find_dkp_tool("elf2nro");
	if (elf2nro.is_empty()) {
		return ERR_UNCONFIGURED;
	}

	PackedStringArray args;
	args.push_back(p_template_elf);
	args.push_back(p_out_path);
	if (!p_icon_path.is_empty()) {
		args.push_back("--icon=" + p_icon_path);
	}
	if (!p_nacp_path.is_empty()) {
		args.push_back("--nacp=" + p_nacp_path);
	}
	if (!p_romfs_dir.is_empty()) {
		args.push_back("--romfsdir=" + p_romfs_dir);
	}

	Array output;
	int exit_code = OS::get_singleton()->execute(elf2nro, args, output, true);
	if (exit_code != 0) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error convert_icon(const String &p_src_icon_path, int p_size, const String &p_out_jpg_path) {
	if (p_size <= 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (!FileAccess::file_exists(p_src_icon_path)) {
		return ERR_CANT_OPEN;
	}

	ProjectSettings *ps = ProjectSettings::get_singleton();
	Ref<Image> src;
	src.instantiate();
	if (src->load(ps->globalize_path(p_src_icon_path)) != OK || src->is_empty()) {
		return ERR_CANT_OPEN;
	}

	src->decompress();
	src->convert(Image::FORMAT_RGBA8);
	src->resize(p_size, p_size, Image::INTERPOLATE_LANCZOS);

	Ref<Image> bg = Image::create_empty(p_size, p_size, false, Image::FORMAT_RGBA8);
	bg->fill(Color(0.0f, 0.0f, 0.0f, 1.0f));
	bg->blend_rect(src, Rect2i(0, 0, p_size, p_size), Vector2i(0, 0));
	bg->convert(Image::FORMAT_RGB8);

	return bg->save_jpg(p_out_jpg_path, 0.92f);
}

static void _remove_tmp_dir_recursive(const String &p_abs_path) {
	if (p_abs_path.is_empty()) return;
	if (!DirAccess::dir_exists_absolute(p_abs_path)) return;

	Ref<DirAccess> dir = DirAccess::open(p_abs_path);
	if (dir.is_null()) return;

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name != "." && name != "..") {
			String child = p_abs_path.path_join(name);
			if (dir->current_is_dir()) {
				_remove_tmp_dir_recursive(child);
			} else {
				DirAccess::remove_absolute(child);
			}
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
	dir.unref();

	DirAccess::remove_absolute(p_abs_path);
}

void remove_tmp_dir(const String &p_abs_path) {
	_remove_tmp_dir_recursive(p_abs_path);
}

void remove_tmp_file(const String &p_abs_path) {
	if (p_abs_path.is_empty()) return;
	if (FileAccess::file_exists(p_abs_path)) {
		DirAccess::remove_absolute(p_abs_path);
	}
}

void ensure_tmp_dir() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	DirAccess::make_dir_recursive_absolute(ps->globalize_path(TMP_DIR));
}

void cleanup_switch_tmp() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	String tmp_dir_abs = ps->globalize_path(TMP_DIR);
	remove_tmp_dir(tmp_dir_abs);

	String godot_dir = ps->globalize_path("res://.godot");
	remove_tmp_file(godot_dir.path_join("switch_pck_entry.tmp"));
}

} // namespace SwitchUtils
