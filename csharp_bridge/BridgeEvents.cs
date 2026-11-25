using System;
using System.Reflection;
using Godot;
using Godot.Collections;

public partial class GdaCSharpBridge : RefCounted
{

    private static ulong _nextGlobalToken = 1;
    private readonly System.Collections.Generic.Dictionary<ulong, Subscription> _subscriptions = new System.Collections.Generic.Dictionary<ulong, Subscription>();

    private static ulong AllocateToken()
    {
        return System.Threading.Interlocked.Increment(ref _nextGlobalToken) - 1;
    }

    private sealed class Subscription
    {
        public GodotObject Source;
        public EventInfo Event;
        public Delegate Handler;
        public Forwarder Relay;
    }

    private sealed class Forwarder
    {
        private readonly GodotObject _target;
        private readonly StringName _method;
        private string _lastError = string.Empty;
        private int _failureCount;

        public Forwarder(GodotObject target, StringName method)
        {
            _target = target;
            _method = method;
        }

        public string LastError { get { return _lastError; } }

        public int FailureCount { get { return _failureCount; } }

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
            catch (Exception e)
            {
                _lastError = e.Message;
                _failureCount++;
                GD.PushError("GDA event forward to '" + _method + "' failed: " + e.Message);
            }
        }
    }

    public Godot.Collections.Dictionary subscribe(Variant obj, string eventName, Variant target, StringName targetMethod)
    {
        try
        {
            GodotObject source = RequireObject(obj);
            GodotObject sink = RequireObject(target);
            if (sink == null)
            {
                return Fail("event target is null");
            }
            if (!sink.HasMethod(targetMethod))
            {
                return Fail("event target has no callable method '" + targetMethod + "'");
            }
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
            ulong token = AllocateToken();
            Subscription sub = new Subscription();
            sub.Source = source;
            sub.Event = ev;
            sub.Handler = handler;
            sub.Relay = closure;
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

    public Godot.Collections.Dictionary subscriptionHealth(ulong token)
    {
        try
        {
            Subscription sub;
            if (!_subscriptions.TryGetValue(token, out sub))
            {
                return Fail("unknown subscription token " + token);
            }
            bool alive = sub.Source != null && GodotObject.IsInstanceValid(sub.Source);
            Forwarder relay = sub.Relay;
            return new Godot.Collections.Dictionary
            {
                { "ok", true },
                { "token", (long)token },
                { "source_alive", alive },
                { "failures", relay == null ? 0L : (long)relay.FailureCount },
                { "last_error", relay == null ? string.Empty : relay.LastError }
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

    public int unsubscribeAll()
    {
        int removed = 0;
        var tokens = new System.Collections.Generic.List<ulong>(_subscriptions.Keys);
        foreach (ulong token in tokens)
        {
            Subscription sub;
            if (!_subscriptions.TryGetValue(token, out sub)) continue;
            try
            {
                sub.Event.RemoveEventHandler(sub.Source, sub.Handler);
                removed++;
            }
            catch (Exception)
            {
            }
            _subscriptions.Remove(token);
        }
        return removed;
    }
}
