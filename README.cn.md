# 黑神话悟空 C# 加载器

[English](README.md)

> **本项目是 [czastack/B1CSharpLoader](https://github.com/czastack/B1CSharpLoader) 的 fork。**
>
> C# 加载器由 [czastack](https://github.com/czastack) 创建，几乎所有代码都由他编写，功劳归原作者所有。
>
> 原项目自 2024 年 10 月以来没有代码更新。我（[KintaroModding](https://github.com/KintaroModding)）接手了这个项目，让它继续可用：适配新的游戏版本、修复 bug，并收集其他人提交给原项目的修复。我不是本项目的作者，只是这个 fork 的维护者。

## 关于

黑神话悟空使用 USharp 作为脚本引擎，很多游戏逻辑使用 C# 实现（见 [GameDll](GameDll)）。其中一些接口只能通过 C# 访问，无法通过 UE C++ 或 Lua 访问。

C# 加载器可以加载用 C# 编写的 mod。C# mod 可以：

- 调用游戏内部的 C# 接口，以及 USharp 包装的虚幻引擎接口，
- 往游戏内部的事件和委托中添加回调，
- 注册键盘按键和手柄按键的快捷键，
- 通过 [Harmony](https://harmony.pardeike.net) hook 修改游戏函数。

## 安装

1. 下载[最新发布版本](https://github.com/KintaroModding/b1csharploader/releases/latest)的 zip 文件。
2. 把 zip 中的 `b1` 文件夹复制到游戏目录 `BlackMythWukong` 中。
3. 每个 C# mod 放在单独的文件夹中：`CSharpLoader/Mods/<ModName>/<ModName>.dll`，例如 `CSharpLoader/Mods/CSharpExample/CSharpExample.dll`。
4. 启动游戏。

安装后，游戏目录如下：

```
BlackMythWukong/b1/Binaries/Win64/
  CSharpLoader/
    Data/
    Mods/
    Plugins/
    0Harmony.dll
    b1cs.ini
    CSharpManager.bin
    CSharpModBase.dll
    Mono.Cecil.dll
    Mono.Cecil.Mdb.dll
    Mono.Cecil.Pdb.dll
    Mono.Cecil.Rocks.dll
    SharpDX.dll
    SharpDX.XInput.dll
  version.dll
```

卸载时，删除 `version.dll` 和 `CSharpLoader` 文件夹即可。

### 配置

配置文件是 `CSharpLoader/b1cs.ini`。`1` 表示开启，`0` 表示关闭。

- `Develop`：开发模式，在游戏中按 Ctrl+F5 重新加载所有 C# mod。
- `Console`：另外在单独的 Windows 控制台窗口中显示日志，用于排查问题。默认关闭。
- `EnableJit`：开启 JIT 模式，见下文。
- `Overlay`：游戏内控制台，显示日志，见下文。
- `OverlayKey`：打开游戏内控制台的按键，默认 `F10`。可以是 `F1` 到 `F24`、一个字母或数字、`Insert`、`Delete`、`Home`、`End`、`Pause`、`ScrollLock`、`Tilde`，或者虚拟键码，例如 `0xC0`。

### 游戏内控制台

游戏内控制台是游戏上方一个可以点击穿透的独立窗口。它在窗口模式和无边框模式下可见，在独占全屏模式下可能不可见。

- 在游戏中按 F10（或你设置的 `OverlayKey`）打开或关闭。
- Page Up 和 Page Down 可以滚动查看最近 500 行。游戏也会收到这两个按键。
- 标题下方列出已加载的 mod，加载失败的 mod 以红色显示。
- 控制台关闭时，游戏右上角的红色小框会显示新错误的数量。

### 横幅

以下情况游戏上方会显示一个横幅 5 秒：

- mod 加载完成时：显示加载器版本和已加载的 mod 数量，
- 按 Ctrl+F5 之后：显示重新加载的 mod 数量，
- 加载器无法启动时，例如游戏更新之后。

加载器或 mod 失败时横幅为红色；游戏版本号与此加载器版本测试过的版本号不同时为黄色。

### 日志文件

加载器会把所有日志写入 `CSharpLoader/Logs`，每次启动游戏一个文件，保留最近 5 个。报告问题时请附上最新的日志文件。

### JIT 模式

为了支持 C# hook，加载器把游戏 Mono 运行时的执行模式从默认的解释执行模式改成了 JIT（运行时编译执行）模式。JIT 可能会带来一些性能提升，但未经过游戏开发商的测试，可能会影响游戏的稳定性。同时它与一些修改器不兼容，例如风灵月影（FLiNG）修改器会失效。

请根据你使用的 mod 的说明决定：如果没有 mod 需要 hook，可以设置 `EnableJit=0`。

### 与其他 version.dll 插件的兼容

`version.dll` 是常用的插件名字。如果其他插件也使用这个名字，可以把它的 dll 改成任意以 `.dll` 结尾的其他名字，放到 `CSharpLoader/Plugins/` 目录中，加载器会一起加载这些插件。

## 仓库中的项目

- `CSharpLoaderDll`：原生加载器（C++）。游戏把它当作 `version.dll` 加载。它通过 Mono API 在游戏启动后启动 `CSharpManager`。
- `CSharpManager`：mod 管理器，加载和管理 C# mod。
- `CSharpModBase`：提供给 mod 使用的接口。
- `CSharpModExample`：mod 示例。
- `GameDll`：游戏内的 C# dll，mod 项目需要引用这些 dll。
- `extract_dlls.py`：从游戏 exe 更新 `GameDll`。
- `build.ps1`：编译全部项目并打包发布用的 zip。

### 编译

在 PowerShell 中运行 `build.ps1`，会编译全部项目，并把发布用的 zip 打包到 `dist/`：

```
powershell -ExecutionPolicy Bypass -File build.ps1 -Version 1.0.0
```

需要安装 Visual Studio 2022（或其生成工具）的 C++ 工作负载，以及 .NET SDK 8 或更新版本。

### 更新 GameDll

`GameDll` 中是游戏版本 1.0.21.23831 的 C# dll。游戏更新后，可以用脚本从游戏 exe 中重新提取（需要 Python 3）：

```
python extract_dlls.py --exe "<游戏目录>/b1/Binaries/Win64/b1-Win64-Shipping.exe"
```

脚本会更新 `GameDll` 中的 dll，并列出游戏中已经没有的文件。加上 `--dry-run` 只列出，不复制。

然后把 `CSharpManager/CSharpModManager.cs` 中的 `TestedGameBuild` 改为新的版本号，即游戏版本的最后一部分（例如 `23831`），并更新本 README 和 `openmods.json` 中的游戏版本。

## mod 编写教程

建议安装 [.NET 8 SDK](https://dotnet.microsoft.com/) 或更新版本，方便使用最新的语法。开发工具可以使用 VS Code、Visual Studio 或 Rider。可以参考 `CSharpModExample` 示例工程。

### mod 入口

mod 的类需要实现 `ICSharpMod` 接口：

```C#
public class MyMod : ICSharpMod
{
    public string Name => "ModExample";
    public string Version => "0.0.1";

    public void Init()
    {
        Log.Info("Init");
    }

    public void DeInit()
    {
        Log.Info("DeInit");
    }
}
```

管理器会为每个实现了 `ICSharpMod` 的类创建实例，并调用 `Init()`。

控制台输出请使用 `Log.Info()`、`Log.Warn()`、`Log.Error()` 和 `Log.Debug()`。它们会在每行加上时间和级别，与加载器自己的输出一致。

按 Ctrl+F5 重新加载 mod 时，管理器会调用每个已加载 mod 的 `DeInit()`，再重新加载所有 mod。开发模式下，每次重新加载都会把 mod 的 dll 加载成新的 Assembly，旧的 Assembly 仍会保留在进程中。所以请在 `DeInit()` 中结束 mod 的后台线程，并移除事件监听。mod 不需要自己清理按键监听，管理器会处理。

### 按键监听

```C#
Utils.RegisterKeyBind(ModifierKeys.Alt, Key.X, () =>
{

});
```

手柄按键的用法相同。玩家按住组合键时，只有按键最多的那个绑定会触发：

```C#
Utils.RegisterGamePadBind(GamePadButton.LeftShoulder | GamePadButton.A, () =>
{

});
```

### C# hook

参考 [Harmony 文档](https://harmony.pardeike.net/articles/patching.html)。hook 需要 `EnableJit=1`。

在 `Init()` 中调用：

```C#
var harmony = new Harmony("your name");
var assembly = Assembly.GetExecutingAssembly();
harmony.PatchAll(assembly);

// or implying current assembly:
harmony.PatchAll();
```

在 `DeInit()` 中调用：

```C#
harmony.UnpatchAll();
```

### mod 的依赖

mod 依赖的 dll 可以放在 `CSharpLoader/Mods/<ModName>/` 中，或者放在 `CSharpLoader/Mods/Common/` 中，让所有 mod 共用。

## Fork 的更改

与原项目相比的更改：

- `GameDll` 更新到游戏版本 1.0.21.23831，并提供在游戏更新后更新它的脚本。
- 修复 bug：快捷键连按、手柄组合键、错误日志、包含非英文字符的路径，以及游戏退出时的输入线程。
- 提供打包发布 zip 的编译脚本。

从原项目的 pull request 中采用的更改：

- "fix: resolved gamepad combination key trigger issue"，作者 reece15
- "Fix: Handle spaces in paths for move command in CSharpManager.csproj"，作者 TheFBplus
- "Add script to extract embedded .NET assemblies from game exe"，作者 inkydragon

根据原项目的 issue 做的修复：

- "一堆BUG"，作者 bfxh

## 许可证

MIT，见 [LICENSE](LICENSE)。`GameDll` 中的 dll 来自游戏，归游戏开发商所有，只用于编译 mod。
