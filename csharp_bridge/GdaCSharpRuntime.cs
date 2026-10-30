using Godot;

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
