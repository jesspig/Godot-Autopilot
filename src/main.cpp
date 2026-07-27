#include <godot_cpp/godot.hpp>

#ifdef _WIN32
#define GSD_EXPORT __declspec(dllexport)
#else
#define GSD_EXPORT
#endif

extern "C" {
GSD_EXPORT GDExtensionBool GDExtensionEntryPoint(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization)
{
    godot::GDExtensionBinding::InitObject init(p_get_proc_address, p_library, r_initialization);
    return init.init();
}
}
