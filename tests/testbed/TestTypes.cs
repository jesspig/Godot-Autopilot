using System;
using Godot;
using Godot.Collections;

[GlobalClass]
public partial class TestTypes : Resource
{
    [Export]
    public Array<int> Scores { get; set; } = new Array<int>();

    public string ReadOnlyName => "gda-probe";

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
}
