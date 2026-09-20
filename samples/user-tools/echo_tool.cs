// 用户工具约定示例（C#，文档级）。
// 说明：
// ① 需要 .NET 版编辑器，且本脚本须先编译通过——C# 脚本由编辑器编译，未编译时无法被实例化调用。
// ② C# 侧无强类型基类：AutopilotTools 单例以 GodotObject 形式取得，按鸭子类型调用
//    register_tool（方法名与参数形态须与 GDScript 约定一致：定义字典 + 可调用对象）。
// ③ rescan 只扫描 .gd 文件，不会自动加载 .cs——C# 工具需在编译后手动注册
//    （见下 RegisterAutopilotTools，由编辑器插件入口或业务代码调用）。
// ④ 工具名全局唯一：同名工具（含内置 MCP 工具）已存在时会被拒绝注册（返回 -1）。
using Godot;
using Godot.Collections;

public partial class EchoTool : RefCounted
{
	// 手动注册示例（在编辑器插件入口或已在场景树中的 Node 里调用）：
	//   var tool = new EchoTool();
	//   tool.RegisterAutopilotTools(Engine.GetSingleton("AutopilotTools") as GodotObject);
	public void RegisterAutopilotTools(GodotObject api)
	{
		var definition = new Dictionary
		{
			{ "name", "echo_tool" },
			{ "description", "Echo back the input value." },
			{ "category", "User" },
			{ "tags", new Array { "user", "sample", "echo" } },
			{ "side_effect", "" },
			{
				"params",
				new Array
				{
					new Dictionary
					{
						{ "name", "value" },
						{ "type", "string" },
						{ "description", "Value to echo back" },
						{ "required", true },
					},
				}
			},
		};
		api.Call("register_tool", definition, Callable.From<Dictionary, Dictionary>(Invoke));
	}

	private Dictionary Invoke(Dictionary args)
	{
		Variant value = args.TryGetValue("value", out Variant found) ? found : Variant.From("");
		return new Dictionary { { "echo", value } };
	}
}
