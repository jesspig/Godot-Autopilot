using System;
using Godot;
using Godot.Collections;

[GlobalClass]
public partial class TestTypes : Resource
{
    [Export]
    public Array<int> Scores { get; set; } = new Array<int>();

    public string ReadOnlyName => "gda-probe";

    public struct NestedInner
    {
        public int Value;
        public Vector2 Pos;
    }

    public struct NestedOuter
    {
        public NestedInner Inner;
        public Color Tint;
    }

    public NestedOuter ReadOnlyNested => new NestedOuter { Inner = new NestedInner { Value = 3, Pos = new Vector2(1, 2) }, Tint = new Color(1, 0, 0) };

    private int _secret = 7;

    public Vector2 GetOffset(int scale)
    {
        return new Vector2(scale, scale * 2);
    }

    public event Action<int> Pinged;

    public void RaisePinged(int value)
    {
        Pinged?.Invoke(value);
    }

    public string JoinAll(params string[] parts) { return string.Join("|", parts); }
}
