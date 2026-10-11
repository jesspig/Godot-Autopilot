using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

    public Godot.Collections.Dictionary get_prop(Variant obj, StringName name, Variant allowNonPublic)
    {
        try
        {
            GodotObject target = RequireObject(obj);
            string key = name.ToString();
            if (HasEngineProperty(target, key))
            {
                return OkWithValue(WrapResult(target.Get(name), 0));
            }
            Type t = target.GetType();
            BindingFlags flags = ResolveFlags(ReadAllowNonPublic(allowNonPublic));
            PropertyInfo prop = t.GetProperty(key, flags);
            if (prop != null)
            {
                if (prop.GetIndexParameters().Length > 0)
                {
                    return Fail("property '" + key + "' on " + t.FullName + " is an indexer, which is not supported");
                }
                MethodInfo getter = prop.GetGetMethod(true);
                if (getter == null)
                {
                    return Fail("property '" + key + "' on " + t.FullName + " has no getter");
                }
                return OkWithValue(WrapResult(prop.GetValue(getter.IsStatic ? null : (object)target), 0));
            }
            FieldInfo field = t.GetField(key, flags);
            if (field != null)
            {
                return OkWithValue(WrapResult(field.GetValue(field.IsStatic ? null : (object)target), 0));
            }
            return Fail("property '" + key + "' not found on " + t.FullName);
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
    }

    public Godot.Collections.Dictionary set_prop(Variant obj, StringName name, Variant value, Variant allowNonPublic)
    {
        try
        {
            GodotObject target = RequireObject(obj);
            string key = name.ToString();
            if (HasEngineProperty(target, key))
            {
                target.Set(name, value);
                return Ok();
            }
            Type t = target.GetType();
            BindingFlags flags = ResolveFlags(ReadAllowNonPublic(allowNonPublic));
            PropertyInfo prop = t.GetProperty(key, flags);
            if (prop != null)
            {
                if (prop.GetIndexParameters().Length > 0)
                {
                    return Fail("property '" + key + "' on " + t.FullName + " is an indexer, which is not supported");
                }
                if (!prop.CanWrite)
                {
                    return Fail("property '" + key + "' on " + t.FullName + " is read-only");
                }
                object converted;
                if (!TryConvertArg(value, prop.PropertyType, out converted))
                {
                    return Fail("cannot convert value to " + prop.PropertyType.FullName + " for property '" + key + "'");
                }
                MethodInfo setter = prop.GetSetMethod(true);
                prop.SetValue(setter.IsStatic ? null : (object)target, converted);
                return Ok();
            }
            FieldInfo field = t.GetField(key, flags);
            if (field != null)
            {
                if (field.IsInitOnly)
                {
                    return Fail("field '" + key + "' on " + t.FullName + " is read-only");
                }
                object converted;
                if (!TryConvertArg(value, field.FieldType, out converted))
                {
                    return Fail("cannot convert value to " + field.FieldType.FullName + " for field '" + key + "'");
                }
                field.SetValue(field.IsStatic ? null : (object)target, converted);
                return Ok();
            }
            return Fail("property '" + key + "' not found on " + t.FullName);
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
    }

    private static bool ReadAllowNonPublic(Variant options)
    {
        if (options.VariantType == Variant.Type.Bool)
        {
            return options.AsBool();
        }
        if (options.VariantType == Variant.Type.Array)
        {
            Godot.Collections.Array items = options.AsGodotArray();
            if (items.Count > 3 && items[3].VariantType == Variant.Type.Bool)
            {
                return items[3].AsBool();
            }
            return false;
        }
        if (options.VariantType == Variant.Type.Dictionary)
        {
            Godot.Collections.Dictionary table = options.AsGodotDictionary();
            Variant raw;
            if (table.TryGetValue("allow_non_public", out raw) && raw.VariantType == Variant.Type.Bool)
            {
                return raw.AsBool();
            }
            return false;
        }
        return false;
    }

    private BindingFlags ResolveFlags(bool allowNonPublic)
    {
        if (allowNonPublic)
        {
            return MemberFlags;
        }
        return BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static;
    }
}
