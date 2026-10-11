using System;
using System.Reflection;
using Godot;
using Godot.Collections;

[Tool]
public partial class GdaCSharpBridge : RefCounted
{

    public const int AbiVersion = 1;

    private const BindingFlags MemberFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

    private bool _allowNonPublic = false;

    public Godot.Collections.Dictionary ping()
    {
        return new Godot.Collections.Dictionary
        {
            { "ok", true },
            { "bridge", "csharp" },
            { "abi", AbiVersion }
        };
    }

    private BindingFlags ResolveFlags()
    {
        if (_allowNonPublic)
        {
            return MemberFlags;
        }
        return BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static;
    }

    private static GodotObject RequireObject(Variant v)
    {
        if (v.VariantType != Variant.Type.Object)
        {
            throw new ArgumentException("expected Object, got " + v.VariantType.ToString());
        }
        GodotObject o = v.AsGodotObject();
        if (o == null)
        {
            throw new ArgumentException("object is null or freed");
        }
        return o;
    }

    private static bool HasEngineProperty(GodotObject target, string key)
    {
        Godot.Collections.Array<Godot.Collections.Dictionary> props = target.GetPropertyList();
        for (int i = 0; i < props.Count; i++)
        {
            Godot.Collections.Dictionary info = props[i];
            Variant raw;
            if (info.TryGetValue("name", out raw) && raw.ToString() == key)
            {
                return true;
            }
        }
        return false;
    }

    private static Godot.Collections.Dictionary Ok()
    {
        return new Godot.Collections.Dictionary { { "ok", true } };
    }

    private static Godot.Collections.Dictionary OkWithValue(Variant value)
    {
        return new Godot.Collections.Dictionary { { "ok", true }, { "value", value } };
    }

    private static Godot.Collections.Dictionary Fail(string message)
    {
        return new Godot.Collections.Dictionary { { "ok", false }, { "error", message } };
    }
}
