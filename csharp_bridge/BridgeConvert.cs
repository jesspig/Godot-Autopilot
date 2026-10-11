using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

    private static bool TryConvertArg(Variant v, Type t, out object result)
    {
        result = null;
        if (t == typeof(Variant))
        {
            result = v;
            return true;
        }
        Type underlying = Nullable.GetUnderlyingType(t);
        if (underlying != null)
        {
            t = underlying;
        }
        if (v.VariantType == Variant.Type.Nil)
        {
            if (!t.IsValueType)
            {
                result = null;
                return true;
            }
            return false;
        }
        if (t == typeof(object))
        {
            result = ToPlainObject(v);
            return true;
        }
        if (typeof(GodotObject).IsAssignableFrom(t))
        {
            if (v.VariantType != Variant.Type.Object)
            {
                return false;
            }
            GodotObject o = v.AsGodotObject();
            if (o == null)
            {
                result = null;
                return true;
            }
            if (t.IsInstanceOfType(o))
            {
                result = o;
                return true;
            }
            return false;
        }
        if (t == typeof(string))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.StringName)
            {
                result = v.ToString();
                return true;
            }
            return false;
        }
        if (t == typeof(StringName))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.StringName)
            {
                result = new StringName(v.ToString());
                return true;
            }
            return false;
        }
        if (t == typeof(NodePath))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.NodePath)
            {
                result = new NodePath(v.ToString());
                return true;
            }
            return false;
        }
        if (t == typeof(bool))
        {
            if (v.VariantType == Variant.Type.Bool)
            {
                result = v.AsBool();
                return true;
            }
            return false;
        }
        if (t.IsEnum)
        {
            try
            {
                if (v.VariantType == Variant.Type.Int)
                {
                    result = Enum.ToObject(t, v.AsInt64());
                    return true;
                }
                if (v.VariantType == Variant.Type.String)
                {
                    result = Enum.Parse(t, v.AsString());
                    return true;
                }
            }
            catch
            {
            }
            return false;
        }
        if (v.VariantType == Variant.Type.Int)
        {
            if (t == typeof(float) || t == typeof(double) || t == typeof(decimal))
            {
                return AssignNumber(System.Convert.ToDouble(v.AsInt64()), t, out result);
            }
            return AssignNumber(v.AsInt64(), t, out result);
        }
        if (v.VariantType == Variant.Type.Float)
        {
            if (t == typeof(float) || t == typeof(double) || t == typeof(decimal))
            {
                return AssignNumber(v.AsDouble(), t, out result);
            }
            return false;
        }
        if (t == typeof(Godot.Collections.Array) && v.VariantType == Variant.Type.Array)
        {
            result = v.AsGodotArray();
            return true;
        }
        if (t == typeof(Godot.Collections.Dictionary) && v.VariantType == Variant.Type.Dictionary)
        {
            result = v.AsGodotDictionary();
            return true;
        }
        if (t.Namespace == "Godot" && t.IsValueType)
        {
            object boxed = BoxGodotValue(v);
            if (boxed != null && t.IsInstanceOfType(boxed))
            {
                result = boxed;
                return true;
            }
            return false;
        }
        return false;
    }

    private static bool AssignNumber(object raw, Type t, out object result)
    {
        result = null;
        try
        {
            if (t == typeof(sbyte) || t == typeof(byte) || t == typeof(short) || t == typeof(ushort) || t == typeof(int) || t == typeof(uint) || t == typeof(long) || t == typeof(ulong) || t == typeof(float) || t == typeof(double) || t == typeof(decimal) || t == typeof(char))
            {
                result = System.Convert.ChangeType(raw, t);
                return true;
            }
        }
        catch
        {
        }
        return false;
    }

    private static object ToPlainObject(Variant v)
    {
        switch (v.VariantType)
        {
            case Variant.Type.Nil:
                return null;
            case Variant.Type.Bool:
                return v.AsBool();
            case Variant.Type.Int:
                return v.AsInt64();
            case Variant.Type.Float:
                return v.AsDouble();
            case Variant.Type.String:
                return v.AsString();
            case Variant.Type.Object:
                return v.AsGodotObject();
            case Variant.Type.Array:
                return v.AsGodotArray();
            case Variant.Type.Dictionary:
                return v.AsGodotDictionary();
            default:
                return BoxGodotValue(v);
        }
    }

    private static object BoxGodotValue(Variant v)
    {
        switch (v.VariantType)
        {
            case Variant.Type.Vector2:
                return v.AsVector2();
            case Variant.Type.Vector2I:
                return v.AsVector2I();
            case Variant.Type.Rect2:
                return v.AsRect2();
            case Variant.Type.Rect2I:
                return v.AsRect2I();
            case Variant.Type.Vector3:
                return v.AsVector3();
            case Variant.Type.Vector3I:
                return v.AsVector3I();
            case Variant.Type.Vector4:
                return v.AsVector4();
            case Variant.Type.Vector4I:
                return v.AsVector4I();
            case Variant.Type.Plane:
                return v.AsPlane();
            case Variant.Type.Quaternion:
                return v.AsQuaternion();
            case Variant.Type.Aabb:
                return v.AsAabb();
            case Variant.Type.Basis:
                return v.AsBasis();
            case Variant.Type.Transform2D:
                return v.AsTransform2D();
            case Variant.Type.Transform3D:
                return v.AsTransform3D();
            case Variant.Type.Projection:
                return v.AsProjection();
            case Variant.Type.Color:
                return v.AsColor();
            case Variant.Type.StringName:
                return v.AsStringName();
            case Variant.Type.NodePath:
                return v.AsNodePath();
            case Variant.Type.Rid:
                return v.AsRid();
            case Variant.Type.Callable:
                return v.AsCallable();
            case Variant.Type.Signal:
                return v.AsSignal();
            case Variant.Type.PackedByteArray:
                return v.AsByteArray();
            case Variant.Type.PackedInt32Array:
                return v.AsInt32Array();
            case Variant.Type.PackedInt64Array:
                return v.AsInt64Array();
            case Variant.Type.PackedFloat32Array:
                return v.AsFloat32Array();
            case Variant.Type.PackedFloat64Array:
                return v.AsFloat64Array();
            case Variant.Type.PackedStringArray:
                return v.AsStringArray();
            case Variant.Type.PackedVector2Array:
                return v.AsVector2Array();
            case Variant.Type.PackedVector3Array:
                return v.AsVector3Array();
            case Variant.Type.PackedColorArray:
                return v.AsColorArray();
            case Variant.Type.PackedVector4Array:
                return v.AsVector4Array();
            default:
                return null;
        }
    }

    private static Variant WrapResult(object raw, int depth)
    {
        if (raw == null)
        {
            return default;
        }
        if (raw is Variant)
        {
            return (Variant)raw;
        }
        Type t = raw.GetType();
        if (t.IsEnum)
        {
            return System.Convert.ToInt64(raw);
        }
        if (t.IsPrimitive || t == typeof(string) || t == typeof(decimal))
        {
            return ToVariant(raw);
        }
        if (raw is GodotObject || raw is Godot.Collections.Array || raw is Godot.Collections.Dictionary)
        {
            return ToVariant(raw);
        }
        if (t.Namespace == "Godot")
        {
            return ToVariant(raw);
        }
        if (t.IsValueType)
        {
            if (depth > 4)
            {
                return Truncated(t, depth);
            }
            return StructDict(raw, t, depth);
        }
        return ToVariant(raw);
    }

    private static Variant ToVariant(object raw)
    {
        if (raw is GodotObject)
        {
            return (GodotObject)raw;
        }
        if (raw is Godot.Collections.Array)
        {
            return (Godot.Collections.Array)raw;
        }
        if (raw is Godot.Collections.Dictionary)
        {
            return (Godot.Collections.Dictionary)raw;
        }
        if (raw is string)
        {
            return (string)raw;
        }
        if (raw is bool)
        {
            return (bool)raw;
        }
        if (raw is sbyte || raw is byte || raw is short || raw is ushort || raw is int || raw is uint || raw is long || raw is ulong)
        {
            return System.Convert.ToInt64(raw);
        }
        if (raw is float || raw is double || raw is decimal)
        {
            return System.Convert.ToDouble(raw);
        }
        if (raw is StringName)
        {
            return (StringName)raw;
        }
        if (raw is NodePath)
        {
            return (NodePath)raw;
        }
        if (raw is Rid)
        {
            return (Rid)raw;
        }
        if (raw is Callable)
        {
            return (Callable)raw;
        }
        if (raw is Signal)
        {
            return (Signal)raw;
        }
        if (raw is Vector2)
        {
            return (Vector2)raw;
        }
        if (raw is Vector2I)
        {
            return (Vector2I)raw;
        }
        if (raw is Rect2)
        {
            return (Rect2)raw;
        }
        if (raw is Rect2I)
        {
            return (Rect2I)raw;
        }
        if (raw is Vector3)
        {
            return (Vector3)raw;
        }
        if (raw is Vector3I)
        {
            return (Vector3I)raw;
        }
        if (raw is Vector4)
        {
            return (Vector4)raw;
        }
        if (raw is Vector4I)
        {
            return (Vector4I)raw;
        }
        if (raw is Plane)
        {
            return (Plane)raw;
        }
        if (raw is Quaternion)
        {
            return (Quaternion)raw;
        }
        if (raw is Aabb)
        {
            return (Aabb)raw;
        }
        if (raw is Basis)
        {
            return (Basis)raw;
        }
        if (raw is Transform2D)
        {
            return (Transform2D)raw;
        }
        if (raw is Transform3D)
        {
            return (Transform3D)raw;
        }
        if (raw is Projection)
        {
            return (Projection)raw;
        }
        if (raw is Color)
        {
            return (Color)raw;
        }
        if (raw is byte[])
        {
            return (byte[])raw;
        }
        if (raw is int[])
        {
            return (int[])raw;
        }
        if (raw is long[])
        {
            return (long[])raw;
        }
        if (raw is float[])
        {
            return (float[])raw;
        }
        if (raw is double[])
        {
            return (double[])raw;
        }
        if (raw is string[])
        {
            return (string[])raw;
        }
        if (raw is Vector2[])
        {
            return (Vector2[])raw;
        }
        if (raw is Vector3[])
        {
            return (Vector3[])raw;
        }
        if (raw is Color[])
        {
            return (Color[])raw;
        }
        if (raw is Vector4[])
        {
            return (Vector4[])raw;
        }
        return raw.ToString();
    }

    private static Godot.Collections.Dictionary StructDict(object raw, Type t, int depth)
    {
        Godot.Collections.Dictionary fields = new Godot.Collections.Dictionary();
        FieldInfo[] fis = t.GetFields(BindingFlags.Public | BindingFlags.Instance);
        for (int i = 0; i < fis.Length; i++)
        {
            object val;
            try
            {
                val = fis[i].GetValue(raw);
            }
            catch (Exception e)
            {
                val = e.Message;
            }
            fields[fis[i].Name] = WrapResult(val, depth + 1);
        }
        PropertyInfo[] pis = t.GetProperties(BindingFlags.Public | BindingFlags.Instance);
        for (int i = 0; i < pis.Length; i++)
        {
            if (!pis[i].CanRead || pis[i].GetIndexParameters().Length > 0)
            {
                continue;
            }
            object val;
            try
            {
                val = pis[i].GetValue(raw);
            }
            catch (Exception e)
            {
                val = e.Message;
            }
            fields[pis[i].Name] = WrapResult(val, depth + 1);
        }
        return new Godot.Collections.Dictionary
        {
            { "__csharp_struct", true },
            { "type", t.FullName },
            { "fields", fields }
        };
    }

    private static Godot.Collections.Dictionary Truncated(Type t, int depth)
    {
        return new Godot.Collections.Dictionary
        {
            { "__truncated", true },
            { "type", t.FullName },
            { "__depth", depth }
        };
    }
}
