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
    private ulong _nextToken = 1;
    private readonly System.Collections.Generic.Dictionary<ulong, Subscription> _subscriptions = new System.Collections.Generic.Dictionary<ulong, Subscription>();

    private sealed class Subscription
    {
        public GodotObject Source;
        public EventInfo Event;
        public Delegate Handler;
    }

    private sealed class Forwarder
    {
        private readonly GodotObject _target;
        private readonly StringName _method;

        public Forwarder(GodotObject target, StringName method)
        {
            _target = target;
            _method = method;
        }

        public void F0()
        {
            Forward(System.Array.Empty<object>());
        }

        public void F1(object a0)
        {
            Forward(new object[] { a0 });
        }

        public void F2(object a0, object a1)
        {
            Forward(new object[] { a0, a1 });
        }

        public void F3(object a0, object a1, object a2)
        {
            Forward(new object[] { a0, a1, a2 });
        }

        public void F4(object a0, object a1, object a2, object a3)
        {
            Forward(new object[] { a0, a1, a2, a3 });
        }

        private void Forward(object[] eventArgs)
        {
            try
            {
                Variant[] callArgs = new Variant[eventArgs.Length];
                for (int i = 0; i < eventArgs.Length; i++)
                {
                    callArgs[i] = WrapResult(eventArgs[i], 1);
                }
                _target.Call(_method, callArgs);
            }
            catch
            {
            }
        }
    }

    public Dictionary ping()
    {
        return new Dictionary
        {
            { "ok", true },
            { "bridge", "csharp" },
            { "abi", AbiVersion }
        };
    }

    public Dictionary get_prop(Variant obj, StringName name)
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
            BindingFlags flags = ResolveFlags();
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

    public Dictionary set_prop(Variant obj, StringName name, Variant value)
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
            BindingFlags flags = ResolveFlags();
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

    public Dictionary call_method(Variant obj, StringName method, Array args)
    {
        try
        {
            GodotObject target = RequireObject(obj);
            string key = method.ToString();
            int count = args.Count;
            Variant[] given = new Variant[count];
            for (int i = 0; i < count; i++)
            {
                given[i] = args[i];
            }
            Type t = target.GetType();
            BindingFlags flags = ResolveFlags();
            bool sawParams = false;
            System.Collections.Generic.List<MethodInfo> matches = new System.Collections.Generic.List<MethodInfo>();
            MethodInfo[] all = t.GetMethods(flags);
            for (int i = 0; i < all.Length; i++)
            {
                MethodInfo mi = all[i];
                if (mi.Name != key || mi.IsSpecialName || mi.IsGenericMethodDefinition)
                {
                    continue;
                }
                ParameterInfo[] ps = mi.GetParameters();
                if (ps.Length > 0 && Attribute.IsDefined(ps[ps.Length - 1], typeof(ParamArrayAttribute)))
                {
                    sawParams = true;
                    continue;
                }
                if (ps.Length == count)
                {
                    matches.Add(mi);
                }
            }
            if (matches.Count == 0)
            {
                if (sawParams)
                {
                    return Fail("method '" + key + "' on " + t.FullName + " only has params-array overloads, which are not supported");
                }
                return Fail("no overload of '" + key + "' on " + t.FullName + " takes " + count + " argument(s)");
            }
            string lastError = string.Empty;
            for (int i = 0; i < matches.Count; i++)
            {
                MethodInfo mi = matches[i];
                ParameterInfo[] ps = mi.GetParameters();
                object[] callArgs = new object[count];
                bool convertible = true;
                for (int j = 0; j < count; j++)
                {
                    object one;
                    if (!TryConvertArg(given[j], ps[j].ParameterType, out one))
                    {
                        convertible = false;
                        lastError = "argument " + j + " cannot convert to " + ps[j].ParameterType.FullName;
                        break;
                    }
                    callArgs[j] = one;
                }
                if (!convertible)
                {
                    continue;
                }
                try
                {
                    object result = mi.Invoke(mi.IsStatic ? null : (object)target, callArgs);
                    return OkWithValue(WrapResult(result, 0));
                }
                catch (TargetInvocationException tie)
                {
                    string inner = tie.InnerException != null ? tie.InnerException.Message : tie.Message;
                    return Fail(inner);
                }
            }
            return Fail("no overload of '" + key + "' on " + t.FullName + " matches: " + lastError);
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
    }

    public Dictionary subscribe(Variant obj, string eventName, Variant target, StringName targetMethod)
    {
        try
        {
            GodotObject source = RequireObject(obj);
            GodotObject sink = RequireObject(target);
            Type t = source.GetType();
            EventInfo ev = t.GetEvent(eventName, ResolveFlags());
            if (ev == null)
            {
                return Fail("event '" + eventName + "' not found on " + t.FullName);
            }
            MethodInfo invoke = ev.EventHandlerType.GetMethod("Invoke");
            ParameterInfo[] ps = invoke.GetParameters();
            if (invoke.ReturnType != typeof(void))
            {
                return Fail("event '" + eventName + "' handler must return void");
            }
            if (ps.Length > 4)
            {
                return Fail("event '" + eventName + "' has " + ps.Length + " parameters; at most 4 are supported");
            }
            MethodInfo forwarder = typeof(Forwarder).GetMethod("F" + ps.Length, MemberFlags);
            if (forwarder == null)
            {
                return Fail("event '" + eventName + "' has " + ps.Length + " parameters; at most 4 are supported");
            }
            Forwarder closure = new Forwarder(sink, targetMethod);
            Delegate handler;
            try
            {
                handler = Delegate.CreateDelegate(ev.EventHandlerType, closure, forwarder);
            }
            catch (Exception e)
            {
                return Fail("cannot bind event '" + eventName + "' (" + ev.EventHandlerType.FullName + "): " + e.Message);
            }
            ev.AddEventHandler(source, handler);
            ulong token = _nextToken;
            _nextToken = _nextToken + 1;
            Subscription sub = new Subscription();
            sub.Source = source;
            sub.Event = ev;
            sub.Handler = handler;
            _subscriptions[token] = sub;
            return new Dictionary
            {
                { "ok", true },
                { "token", (long)token }
            };
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
    }

    public Dictionary unsubscribe(ulong token)
    {
        try
        {
            Subscription sub;
            if (!_subscriptions.TryGetValue(token, out sub))
            {
                return Fail("unknown subscription token " + token);
            }
            sub.Event.RemoveEventHandler(sub.Source, sub.Handler);
            _subscriptions.Remove(token);
            return Ok();
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
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
        Array props = target.GetPropertyList();
        for (int i = 0; i < props.Count; i++)
        {
            Dictionary info = props[i].AsGodotDictionary();
            Variant raw;
            if (info.TryGetValue("name", out raw) && raw.ToString() == key)
            {
                return true;
            }
        }
        return false;
    }

    private static Dictionary Ok()
    {
        return new Dictionary { { "ok", true } };
    }

    private static Dictionary OkWithValue(Variant value)
    {
        return new Dictionary { { "ok", true }, { "value", value } };
    }

    private static Dictionary Fail(string message)
    {
        return new Dictionary { { "ok", false }, { "error", message } };
    }

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
        if (t == typeof(Array) && v.VariantType == Variant.Type.Array)
        {
            result = v.AsGodotArray();
            return true;
        }
        if (t == typeof(Dictionary) && v.VariantType == Variant.Type.Dictionary)
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
        if (raw is GodotObject || raw is Array || raw is Dictionary)
        {
            return ToVariant(raw);
        }
        if (t.Namespace == "Godot")
        {
            return ToVariant(raw);
        }
        if (t.IsValueType)
        {
            if (depth > 0)
            {
                return Truncated(t);
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
        if (raw is Array)
        {
            return (Array)raw;
        }
        if (raw is Dictionary)
        {
            return (Dictionary)raw;
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
        if (raw is PackedByteArray)
        {
            return (PackedByteArray)raw;
        }
        if (raw is PackedInt32Array)
        {
            return (PackedInt32Array)raw;
        }
        if (raw is PackedInt64Array)
        {
            return (PackedInt64Array)raw;
        }
        if (raw is PackedFloat32Array)
        {
            return (PackedFloat32Array)raw;
        }
        if (raw is PackedFloat64Array)
        {
            return (PackedFloat64Array)raw;
        }
        if (raw is PackedStringArray)
        {
            return (PackedStringArray)raw;
        }
        if (raw is PackedVector2Array)
        {
            return (PackedVector2Array)raw;
        }
        if (raw is PackedVector3Array)
        {
            return (PackedVector3Array)raw;
        }
        if (raw is PackedColorArray)
        {
            return (PackedColorArray)raw;
        }
        if (raw is PackedVector4Array)
        {
            return (PackedVector4Array)raw;
        }
        return raw.ToString();
    }

    private static Dictionary StructDict(object raw, Type t, int depth)
    {
        Dictionary fields = new Dictionary();
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
        return new Dictionary
        {
            { "__csharp_struct", true },
            { "type", t.FullName },
            { "fields", fields }
        };
    }

    private static Dictionary Truncated(Type t)
    {
        return new Dictionary
        {
            { "__truncated", true },
            { "type", t.FullName }
        };
    }
}

public partial class GdaCSharpRuntime : Node
{
    public const string BridgeNodeName = "__gda_runtime";

    private GdaCSharpBridge _bridge;

    public override void _Ready()
    {
        if (GetNodeOrNull(BridgeNodeName) == null)
        {
            Node holder = new Node();
            holder.Name = BridgeNodeName;
            AddChild(holder);
        }
        _bridge = new GdaCSharpBridge();
        GetNode(BridgeNodeName).SetMeta("bridge", _bridge);
    }

    public GdaCSharpBridge GetBridge()
    {
        return _bridge;
    }
}
