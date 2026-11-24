#include "register_types.h"
#include "editor_export_platform_switch.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

class EditorPluginSwitch : public EditorPlugin {
	GDCLASS(EditorPluginSwitch, EditorPlugin);

private:
	Ref<EditorExportPlatformSwitch> export_platform; // ganti dari pointer mentah

protected:
	static void _bind_methods() {}

public:
	void _enter_tree() override {
		if (export_platform.is_null()) {
			export_platform.instantiate();
			add_export_platform(export_platform);
		}
	}

	void _exit_tree() override {
		if (export_platform.is_valid()) {
			remove_export_platform(export_platform);
			export_platform.unref();
		}
	}
};

void initialize_switch_exporter_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}

	GDREGISTER_INTERNAL_CLASS(EditorPluginSwitch);
	GDREGISTER_INTERNAL_CLASS(EditorExportPlatformSwitch);

	EditorPlugins::add_by_type<EditorPluginSwitch>();
}

void uninitialize_switch_exporter_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}

	EditorPlugins::remove_by_type<EditorPluginSwitch>();
}

extern "C" {

// initialization
GDExtensionBool GDE_EXPORT switch_exporter_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_switch_exporter_module);
	init_obj.register_terminator(uninitialize_switch_exporter_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_EDITOR);

	return init_obj.init();
}
}
