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
            System.Collections.Generic.List<int> applicableScores = new System.Collections.Generic.List<int>();
            string lastError = string.Empty;
            for (int i = 0; i < candidates.Count; i++)
            {
                MethodInfo mi = candidates[i];
                ParameterInfo[] ps = mi.GetParameters();
                bool isParams = ps.Length > 0 && Attribute.IsDefined(ps[ps.Length - 1], typeof(ParamArrayAttribute));
                if (!isParams)
                {
                    object[] callArgs = new object[count];
                    int total = 0;
                    bool convertible = true;
                    for (int j = 0; j < count; j++)
                    {
                        object one;
                        int oneCost;
                        if (!TryConvertScored(given[j], ps[j].ParameterType, out one, out oneCost))
                        {
                            convertible = false;
                            lastError = "argument " + j + " cannot convert to " + ps[j].ParameterType.FullName;
                            break;
                        }
                        callArgs[j] = one;
                        total = total + oneCost;
                    }
                    if (!convertible)
                    {
                        continue;
                    }
                    applicable.Add(mi);
                    applicableArgs.Add(callArgs);
                    applicableScores.Add(total);
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
                int paramsTotal = 1;
                bool paramsConvertible = true;
                for (int j = 0; j < fixedCount; j++)
                {
                    object one;
                    int oneCost;
                    if (!TryConvertScored(given[j], ps[j].ParameterType, out one, out oneCost))
                    {
                        paramsConvertible = false;
                        lastError = "argument " + j + " cannot convert to " + ps[j].ParameterType.FullName;
                        break;
                    }
                    paramsArgs[j] = one;
                    paramsTotal = paramsTotal + oneCost;
                }
                if (!paramsConvertible)
                {
                    continue;
                }
                int rest = count - fixedCount;
                System.Array packed = System.Array.CreateInstance(elementType, rest);
                int packedMax = 0;
                for (int j = 0; j < rest; j++)
                {
                    object one;
                    int oneCost;
                    if (!TryConvertScored(given[fixedCount + j], elementType, out one, out oneCost))
                    {
                        paramsConvertible = false;
                        lastError = "argument " + (fixedCount + j) + " cannot convert to " + elementType.FullName;
                        break;
                    }
                    packed.SetValue(one, j);
                    if (oneCost > packedMax)
                    {
                        packedMax = oneCost;
                    }
                }
                if (!paramsConvertible)
                {
                    continue;
                }
                paramsArgs[fixedCount] = packed;
                applicable.Add(mi);
                applicableArgs.Add(paramsArgs);
                applicableScores.Add(paramsTotal + packedMax);
            }
            if (applicable.Count == 0)
            {
                return Fail("no overload of '" + key + "' on " + t.FullName + " matches: " + lastError);
            }
            int best = 0;
            for (int i = 1; i < applicable.Count; i++)
            {
                if (applicableScores[i] < applicableScores[best])
                {
                    best = i;
                }
            }
            int tied = 0;
            for (int i = 0; i < applicable.Count; i++)
            {
                if (applicableScores[i] == applicableScores[best])
                {
                    tied = tied + 1;
                }
            }
            if (tied > 1)
            {
                return Fail("ambiguous call to '" + key + "' on " + t.FullName + " with " + count + " argument(s): " + tied + " overload(s) match");
            }
            try
            {
                MethodInfo winner = applicable[best];
                object result = winner.Invoke(winner.IsStatic ? null : (object)target, applicableArgs[best]);
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
