using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

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

        public void Forward()
        {
            ForwardCore(System.Array.Empty<object>());
        }

        public void Forward<T0>(T0 a0)
        {
            ForwardCore(new object[] { a0 });
        }

        public void Forward<T0, T1>(T0 a0, T1 a1)
        {
            ForwardCore(new object[] { a0, a1 });
        }

        public void Forward<T0, T1, T2>(T0 a0, T1 a1, T2 a2)
        {
            ForwardCore(new object[] { a0, a1, a2 });
        }

        public void Forward<T0, T1, T2, T3>(T0 a0, T1 a1, T2 a2, T3 a3)
        {
            ForwardCore(new object[] { a0, a1, a2, a3 });
        }

        private void ForwardCore(object[] eventArgs)
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

    public Godot.Collections.Dictionary subscribe(Variant obj, string eventName, Variant target, StringName targetMethod)
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
            MethodInfo forwarder = null;
            MethodInfo[] forwarders = typeof(Forwarder).GetMethods(MemberFlags);
            for (int i = 0; i < forwarders.Length; i++)
            {
                MethodInfo cand = forwarders[i];
                if (cand.Name != "Forward" || cand.GetParameters().Length != ps.Length)
                {
                    continue;
                }
                if (ps.Length == 0 && !cand.IsGenericMethodDefinition)
                {
                    forwarder = cand;
                    break;
                }
                if (ps.Length > 0 && cand.IsGenericMethodDefinition && cand.GetGenericArguments().Length == ps.Length)
                {
                    forwarder = cand;
                    break;
                }
            }
            if (forwarder == null)
            {
                return Fail("event '" + eventName + "' has " + ps.Length + " parameters; at most 4 are supported");
            }
            if (ps.Length > 0)
            {
                Type[] typeArgs = new Type[ps.Length];
                for (int i = 0; i < ps.Length; i++)
                {
                    typeArgs[i] = ps[i].ParameterType;
                }
                forwarder = forwarder.MakeGenericMethod(typeArgs);
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
            return new Godot.Collections.Dictionary
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

    public Godot.Collections.Dictionary unsubscribe(ulong token)
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
}
