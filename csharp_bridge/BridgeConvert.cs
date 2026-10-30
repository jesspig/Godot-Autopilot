using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

    private static bool TryConvertArg(Variant v, Type t, out object result)
    {
        int cost;
        return TryConvertScored(v, t, out result, out cost);
    }

    private static bool TryConvertScored(Variant v, Type t, out object result, out int cost)
    {
        result = null;
        cost = 0;
        if (t == typeof(Variant))
        {
            result = v;
            cost = 0;
            return true;
        }
        Type nullableInner = Nullable.GetUnderlyingType(t);
        bool wasNullable = nullableInner != null;
        if (wasNullable)
        {
            t = nullableInner;
        }
        if (v.VariantType == Variant.Type.Nil)
        {
            if (!t.IsValueType || wasNullable)
            {
                result = null;
                cost = 0;
                return true;
            }
            return false;
        }
        if (t == typeof(object))
        {
            result = ToPlainObject(v);
            cost = 4;
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
                cost = 0;
                return true;
            }
            if (t.IsInstanceOfType(o))
            {
                result = o;
                cost = o.GetType() == t ? 0 : 1;
                return true;
            }
            return false;
        }
        if (t == typeof(string))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.StringName)
            {
                result = v.ToString();
                cost = 0;
                return true;
            }
            return false;
        }
        if (t == typeof(StringName))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.StringName)
            {
                result = new StringName(v.ToString());
                cost = 0;
                return true;
            }
            return false;
        }
        if (t == typeof(NodePath))
        {
            if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.NodePath)
            {
                result = new NodePath(v.ToString());
                cost = 0;
                return true;
            }
            return false;
        }
        if (t == typeof(bool))
        {
            if (v.VariantType == Variant.Type.Bool)
            {
                result = v.AsBool();
                cost = 0;
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
                    cost = 1;
                    return true;
                }
                if (v.VariantType == Variant.Type.String || v.VariantType == Variant.Type.StringName)
                {
                    result = Enum.Parse(t, v.ToString(), true);
                    cost = 2;
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
                if (AssignNumber(System.Convert.ToDouble(v.AsInt64()), t, out result))
                {
                    cost = t == typeof(double) ? 0 : 1;
                    return true;
                }
                return false;
            }
            if (AssignNumber(v.AsInt64(), t, out result))
            {
                cost = t == typeof(long) ? 0 : 1;
                return true;
            }
            return false;
        }
        if (v.VariantType == Variant.Type.Float)
        {
            if (t == typeof(float) || t == typeof(double) || t == typeof(decimal))
            {
                if (AssignNumber(v.AsDouble(), t, out result))
                {
                    cost = t == typeof(double) ? 0 : 1;
                    return true;
                }
            }
            return false;
        }
        if (t == typeof(Godot.Collections.Array) && v.VariantType == Variant.Type.Array)
        {
            result = v.AsGodotArray();
            cost = 0;
            return true;
        }
        if (t == typeof(Godot.Collections.Dictionary) && v.VariantType == Variant.Type.Dictionary)
        {
            result = v.AsGodotDictionary();
            cost = 0;
            return true;
        }
        if (t.IsGenericType && t.GetGenericTypeDefinition() == typeof(Godot.Collections.Array<>))
        {
            return TryConvertGodotSequence(v, t.GetGenericArguments()[0], out result, out cost);
        }
        if (t.IsGenericType && t.GetGenericTypeDefinition() == typeof(Godot.Collections.Dictionary<,>))
        {
            Type[] sides = t.GetGenericArguments();
            return TryConvertGodotMapping(v, sides[0], sides[1], out result, out cost);
        }
        Type sequenceElement;
        if (TryGetSequenceElementType(t, out sequenceElement))
        {
            return TryConvertSequence(v, t, sequenceElement, out result, out cost);
        }
        Type mapKey;
        Type mapValue;
        if (TryGetDictionaryKeyValueTypes(t, out mapKey, out mapValue))
        {
            return TryConvertMapping(v, mapKey, mapValue, out result, out cost);
        }
        if (t == typeof(System.Collections.IEnumerable) || t == typeof(System.Collections.IList) || t == typeof(System.Collections.ICollection))
        {
            Variant[] items;
            if (!TryGetVariantItems(v, out items))
            {
                return false;
            }
            System.Collections.ArrayList built = new System.Collections.ArrayList(items.Length);
            for (int i = 0; i < items.Length; i++)
            {
                built.Add(ToPlainObject(items[i]));
            }
            result = built;
            cost = 2;
            return true;
        }
        if (t == typeof(System.Collections.IDictionary))
        {
            if (v.VariantType != Variant.Type.Dictionary)
            {
                return false;
            }
            Godot.Collections.Dictionary raw = v.AsGodotDictionary();
            System.Collections.Hashtable built = new System.Collections.Hashtable();
            foreach (Variant entry in raw.Keys)
            {
                built[ToPlainObject(entry)] = ToPlainObject(raw[entry]);
            }
            result = built;
            cost = 2;
            return true;
        }
        if (t.Namespace == "Godot" && t.IsValueType)
        {
            object boxed = BoxGodotValue(v);
            if (boxed != null && t.IsInstanceOfType(boxed))
            {
                result = boxed;
                cost = 0;
                return true;
            }
            return false;
        }
        return false;
    }

    private static bool TryGetSequenceElementType(Type t, out Type element)
    {
        element = null;
        if (t.IsArray)
        {
            if (t.GetArrayRank() != 1)
            {
                return false;
            }
            element = t.GetElementType();
            return true;
        }
        if (t.IsGenericType)
        {
            Type generic = t.GetGenericTypeDefinition();
            if (generic == typeof(System.Collections.Generic.List<>) || generic == typeof(System.Collections.Generic.IList<>) || generic == typeof(System.Collections.Generic.ICollection<>) || generic == typeof(System.Collections.Generic.IEnumerable<>) || generic == typeof(System.Collections.Generic.IReadOnlyList<>) || generic == typeof(System.Collections.Generic.IReadOnlyCollection<>))
            {
                element = t.GetGenericArguments()[0];
                return true;
            }
        }
        if (t.IsInterface)
        {
            Type found = null;
            Type[] faces = t.GetInterfaces();
            for (int i = 0; i < faces.Length; i++)
            {
                if (faces[i].IsGenericType && faces[i].GetGenericTypeDefinition() == typeof(System.Collections.Generic.IEnumerable<>))
                {
                    if (found != null)
                    {
                        return false;
                    }
                    found = faces[i].GetGenericArguments()[0];
                }
            }
            if (found != null)
            {
                element = found;
                return true;
            }
        }
        return false;
    }

    private static bool TryGetDictionaryKeyValueTypes(Type t, out Type key, out Type value)
    {
        key = null;
        value = null;
        if (!t.IsGenericType)
        {
            return false;
        }
        Type generic = t.GetGenericTypeDefinition();
        if (generic == typeof(System.Collections.Generic.Dictionary<,>) || generic == typeof(System.Collections.Generic.IDictionary<,>) || generic == typeof(System.Collections.Generic.IReadOnlyDictionary<,>))
        {
            Type[] sides = t.GetGenericArguments();
            key = sides[0];
            value = sides[1];
            return true;
        }
        return false;
    }

    private static bool TryGetVariantItems(Variant v, out Variant[] items)
    {
        items = null;
        if (v.VariantType == Variant.Type.Array)
        {
            Godot.Collections.Array raw = v.AsGodotArray();
            items = new Variant[raw.Count];
            for (int i = 0; i < raw.Count; i++)
            {
                items[i] = raw[i];
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedByteArray)
        {
            byte[] raw = v.AsByteArray();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedInt32Array)
        {
            int[] raw = v.AsInt32Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedInt64Array)
        {
            long[] raw = v.AsInt64Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedFloat32Array)
        {
            float[] raw = v.AsFloat32Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedFloat64Array)
        {
            double[] raw = v.AsFloat64Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedStringArray)
        {
            string[] raw = v.AsStringArray();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedVector2Array)
        {
            Vector2[] raw = v.AsVector2Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedVector3Array)
        {
            Vector3[] raw = v.AsVector3Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedColorArray)
        {
            Color[] raw = v.AsColorArray();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        if (v.VariantType == Variant.Type.PackedVector4Array)
        {
            Vector4[] raw = v.AsVector4Array();
            items = new Variant[raw.Length];
            for (int i = 0; i < raw.Length; i++)
            {
                items[i] = Variant.From(raw[i]);
            }
            return true;
        }
        return false;
    }

    private static bool TryConvertSequence(Variant v, Type t, Type element, out object result, out int cost)
    {
        result = null;
        cost = 0;
        Variant[] items;
        if (!TryGetVariantItems(v, out items))
        {
            return false;
        }
        if (t.IsArray)
        {
            System.Array built = System.Array.CreateInstance(element, items.Length);
            int worst = 0;
            for (int i = 0; i < items.Length; i++)
            {
                object one;
                int oneCost;
                if (!TryConvertScored(items[i], element, out one, out oneCost))
                {
                    return false;
                }
                built.SetValue(one, i);
                if (oneCost > worst)
                {
                    worst = oneCost;
                }
            }
            result = built;
            cost = 2 + worst;
            return true;
        }
        object made;
        try
        {
            made = System.Activator.CreateInstance(typeof(System.Collections.Generic.List<>).MakeGenericType(element));
        }
        catch
        {
            return false;
        }
        System.Collections.IList table = made as System.Collections.IList;
        if (table == null)
        {
            return false;
        }
        int peak = 0;
        for (int i = 0; i < items.Length; i++)
        {
            object one;
            int oneCost;
            if (!TryConvertScored(items[i], element, out one, out oneCost))
            {
                return false;
            }
            table.Add(one);
            if (oneCost > peak)
            {
                peak = oneCost;
            }
        }
        result = made;
        cost = 2 + peak;
        return true;
    }

    private static bool TryConvertMapping(Variant v, Type key, Type value, out object result, out int cost)
    {
        result = null;
        cost = 0;
        if (v.VariantType != Variant.Type.Dictionary)
        {
            return false;
        }
        Godot.Collections.Dictionary raw = v.AsGodotDictionary();
        object made;
        try
        {
            made = System.Activator.CreateInstance(typeof(System.Collections.Generic.Dictionary<,>).MakeGenericType(key, value));
        }
        catch
        {
            return false;
        }
        System.Collections.IDictionary table = made as System.Collections.IDictionary;
        if (table == null)
        {
            return false;
        }
        int worst = 0;
        foreach (Variant entry in raw.Keys)
        {
            object oneKey;
            int keyCost;
            if (!TryConvertScored(entry, key, out oneKey, out keyCost))
            {
                return false;
            }
            object oneValue;
            int valueCost;
            if (!TryConvertScored(raw[entry], value, out oneValue, out valueCost))
            {
                return false;
            }
            table.Add(oneKey, oneValue);
            if (keyCost > worst)
            {
                worst = keyCost;
            }
            if (valueCost > worst)
            {
                worst = valueCost;
            }
        }
        result = made;
        cost = 2 + worst;
        return true;
    }

    private static bool TryConvertGodotSequence(Variant v, Type element, out object result, out int cost)
    {
        result = null;
        cost = 0;
        Variant[] items;
        if (!TryGetVariantItems(v, out items))
        {
            return false;
        }
        Type target = typeof(Godot.Collections.Array<>).MakeGenericType(element);
        object made;
        try
        {
            made = System.Activator.CreateInstance(target);
        }
        catch
        {
            return false;
        }
        MethodInfo add;
        try
        {
            add = target.GetMethod("Add");
        }
        catch
        {
            return false;
        }
        if (add == null)
        {
            return false;
        }
        int worst = 0;
        for (int i = 0; i < items.Length; i++)
        {
            object one;
            int oneCost;
            if (!TryConvertScored(items[i], element, out one, out oneCost))
            {
                return false;
            }
            try
            {
                add.Invoke(made, new object[] { one });
            }
            catch
            {
                return false;
            }
            if (oneCost > worst)
            {
                worst = oneCost;
            }
        }
        result = made;
        cost = 1 + worst;
        return true;
    }

    private static bool TryConvertGodotMapping(Variant v, Type key, Type value, out object result, out int cost)
    {
        result = null;
        cost = 0;
        if (v.VariantType != Variant.Type.Dictionary)
        {
            return false;
        }
        Godot.Collections.Dictionary raw = v.AsGodotDictionary();
        Type target = typeof(Godot.Collections.Dictionary<,>).MakeGenericType(key, value);
        object made;
        try
        {
            made = System.Activator.CreateInstance(target);
        }
        catch
        {
            return false;
        }
        MethodInfo add;
        try
        {
            add = target.GetMethod("Add");
        }
        catch
        {
            return false;
        }
        if (add == null)
        {
            return false;
        }
        int worst = 0;
        foreach (Variant entry in raw.Keys)
        {
            object oneKey;
            int keyCost;
            if (!TryConvertScored(entry, key, out oneKey, out keyCost))
            {
                return false;
            }
            object oneValue;
            int valueCost;
            if (!TryConvertScored(raw[entry], value, out oneValue, out valueCost))
            {
                return false;
            }
            try
            {
                add.Invoke(made, new object[] { oneKey, oneValue });
            }
            catch
            {
                return false;
            }
            if (keyCost > worst)
            {
                worst = keyCost;
            }
            if (valueCost > worst)
            {
                worst = valueCost;
            }
        }
        result = made;
        cost = 1 + worst;
        return true;
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
        if (raw is System.Array && IsPackedResultArray(t))
        {
            return ToVariant(raw);
        }
        if (IsDictionaryLike(t))
        {
            return DictionaryFromEntries(raw, t, depth);
        }
        if (raw is System.Collections.IEnumerable && !(raw is string))
        {
            return ArrayFromSequence(raw, t, depth);
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

    private static bool IsPackedResultArray(Type t)
    {
        if (!t.IsArray || t.GetArrayRank() != 1)
        {
            return false;
        }
        Type element = t.GetElementType();
        return element == typeof(byte) || element == typeof(int) || element == typeof(long) || element == typeof(float) || element == typeof(double) || element == typeof(string) || element == typeof(Vector2) || element == typeof(Vector3) || element == typeof(Color) || element == typeof(Vector4);
    }

    private static bool IsDictionaryLike(Type t)
    {
        if (typeof(System.Collections.IDictionary).IsAssignableFrom(t))
        {
            return true;
        }
        if (t.IsGenericType)
        {
            Type generic = t.GetGenericTypeDefinition();
            if (generic == typeof(System.Collections.Generic.Dictionary<,>) || generic == typeof(Godot.Collections.Dictionary<,>))
            {
                return true;
            }
        }
        Type[] faces = t.GetInterfaces();
        for (int i = 0; i < faces.Length; i++)
        {
            if (faces[i].IsGenericType && faces[i].GetGenericTypeDefinition() == typeof(System.Collections.Generic.IDictionary<,>))
            {
                return true;
            }
        }
        return false;
    }

    private static bool TrySplitKeyValue(object item, out object entryKey, out object entryValue)
    {
        entryKey = null;
        entryValue = null;
        if (item == null)
        {
            return false;
        }
        if (item is System.Collections.DictionaryEntry)
        {
            System.Collections.DictionaryEntry pair = (System.Collections.DictionaryEntry)item;
            entryKey = pair.Key;
            entryValue = pair.Value;
            return true;
        }
        Type itemType = item.GetType();
        if (!itemType.IsGenericType || itemType.GetGenericTypeDefinition() != typeof(System.Collections.Generic.KeyValuePair<,>))
        {
            return false;
        }
        PropertyInfo keyProp = itemType.GetProperty("Key");
        PropertyInfo valueProp = itemType.GetProperty("Value");
        if (keyProp == null || valueProp == null)
        {
            return false;
        }
        entryKey = keyProp.GetValue(item);
        entryValue = valueProp.GetValue(item);
        return true;
    }

    private static Variant DictionaryFromEntries(object raw, Type t, int depth)
    {
        if (depth > 4)
        {
            return Truncated(t, depth);
        }
        Godot.Collections.Dictionary table = new Godot.Collections.Dictionary();
        if (raw is System.Collections.IDictionary)
        {
            System.Collections.IDictionary plain = (System.Collections.IDictionary)raw;
            foreach (System.Collections.DictionaryEntry entry in plain)
            {
                table[WrapResult(entry.Key, depth + 1)] = WrapResult(entry.Value, depth + 1);
            }
            return table;
        }
        foreach (object item in (System.Collections.IEnumerable)raw)
        {
            object entryKey;
            object entryValue;
            if (!TrySplitKeyValue(item, out entryKey, out entryValue))
            {
                return raw.ToString();
            }
            table[WrapResult(entryKey, depth + 1)] = WrapResult(entryValue, depth + 1);
        }
        return table;
    }

    private static Variant ArrayFromSequence(object raw, Type t, int depth)
    {
        if (depth > 4)
        {
            return Truncated(t, depth);
        }
        Godot.Collections.Array seq = new Godot.Collections.Array();
        foreach (object item in (System.Collections.IEnumerable)raw)
        {
            seq.Add(WrapResult(item, depth + 1));
        }
        return seq;
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
