using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

    public Godot.Collections.Dictionary call_method(Variant obj, StringName method, Godot.Collections.Array args)
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
            System.Collections.Generic.List<MethodInfo> candidates = new System.Collections.Generic.List<MethodInfo>();
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
                    if (count >= ps.Length - 1)
                    {
                        candidates.Add(mi);
                    }
                    continue;
                }
                if (ps.Length == count)
                {
                    candidates.Add(mi);
                }
            }
            if (candidates.Count == 0)
            {
                return Fail("no overload of '" + key + "' on " + t.FullName + " takes " + count + " argument(s)");
            }
            System.Collections.Generic.List<MethodInfo> applicable = new System.Collections.Generic.List<MethodInfo>();
            System.Collections.Generic.List<object[]> applicableArgs = new System.Collections.Generic.List<object[]>();
            string lastError = string.Empty;
            for (int i = 0; i < candidates.Count; i++)
            {
                MethodInfo mi = candidates[i];
                ParameterInfo[] ps = mi.GetParameters();
                bool isParams = ps.Length > 0 && Attribute.IsDefined(ps[ps.Length - 1], typeof(ParamArrayAttribute));
                if (!isParams)
                {
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
                    applicable.Add(mi);
                    applicableArgs.Add(callArgs);
                    continue;
                }
                int fixedCount = ps.Length - 1;
                Type elementType = ps[ps.Length - 1].ParameterType.GetElementType();
                if (elementType == null)
                {
                    lastError = "params parameter of '" + key + "' is not an array";
                    continue;
                }
                object[] paramsArgs = new object[ps.Length];
                bool paramsConvertible = true;
                for (int j = 0; j < fixedCount; j++)
                {
                    object one;
                    if (!TryConvertArg(given[j], ps[j].ParameterType, out one))
                    {
                        paramsConvertible = false;
                        lastError = "argument " + j + " cannot convert to " + ps[j].ParameterType.FullName;
                        break;
                    }
                    paramsArgs[j] = one;
                }
                if (!paramsConvertible)
                {
                    continue;
                }
                int rest = count - fixedCount;
                System.Array packed = System.Array.CreateInstance(elementType, rest);
                for (int j = 0; j < rest; j++)
                {
                    object one;
                    if (!TryConvertArg(given[fixedCount + j], elementType, out one))
                    {
                        paramsConvertible = false;
                        lastError = "argument " + (fixedCount + j) + " cannot convert to " + elementType.FullName;
                        break;
                    }
                    packed.SetValue(one, j);
                }
                if (!paramsConvertible)
                {
                    continue;
                }
                paramsArgs[fixedCount] = packed;
                applicable.Add(mi);
                applicableArgs.Add(paramsArgs);
            }
            if (applicable.Count == 0)
            {
                return Fail("no overload of '" + key + "' on " + t.FullName + " matches: " + lastError);
            }
            if (applicable.Count > 1)
            {
                return Fail("ambiguous call to '" + key + "' on " + t.FullName + " with " + count + " argument(s): " + applicable.Count + " overload(s) match");
            }
            try
            {
                MethodInfo best = applicable[0];
                object result = best.Invoke(best.IsStatic ? null : (object)target, applicableArgs[0]);
                return OkWithValue(WrapResult(result, 0));
            }
            catch (TargetInvocationException tie)
            {
                string inner = tie.InnerException != null ? tie.InnerException.Message : tie.Message;
                return Fail(inner);
            }
        }
        catch (Exception e)
        {
            return Fail(e.Message);
        }
    }
}
