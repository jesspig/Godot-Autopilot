using Godot;

namespace GDExtensionBindgen
{
    public sealed class GdextensionHandle
    {
        private readonly GodotObject _target;

        public GdextensionHandle(GodotObject target)
        {
            _target = target;
        }

        public GodotObject Target
        {
            get { return _target; }
        }

        public bool IsLibraryOpen()
        {
            return _target.Call("is_library_open").AsBool();
        }

        public int GetMinimumLibraryInitializationLevel()
        {
            return _target.Call("get_minimum_library_initialization_level").AsInt32();
        }
    }

    public static class GdextensionManager
    {
        private static GodotObject Singleton()
        {
            return Engine.GetSingleton("GDExtensionManager");
        }

        public static bool LoadExtension(string path)
        {
            return Singleton().Call("load_extension", path).AsBool();
        }

        public static bool IsExtensionLoaded(string path)
        {
            return Singleton().Call("is_extension_loaded", path).AsBool();
        }

        public static string[] GetLoadedExtensions()
        {
            return Singleton().Call("get_loaded_extensions").AsStringArray();
        }
    }
}
