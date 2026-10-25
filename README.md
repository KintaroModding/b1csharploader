# C# Loader for Black Myth: Wukong

[中文](README.cn.md)

> **This is a fork of [czastack/B1CSharpLoader](https://github.com/czastack/B1CSharpLoader).**
>
> [czastack](https://github.com/czastack) created C# Loader and wrote almost all of its code. All credit for the loader goes to them.
>
> The original project has had no code changes since October 2024. I ([KintaroModding](https://github.com/KintaroModding)) took it over to keep it alive: I keep it working with new game versions, fix bugs, and collect the fixes that other people sent to the original project. I am not the creator of this project, only the maintainer of this fork.

## About

Black Myth: Wukong uses USharp as its script engine, so much of the game's logic is written in C# (see [GameDll](GameDll)). Some of these APIs can only be reached from C#, not from Unreal Engine C++ or Lua.

C# Loader loads mods that are written in C#. A C# mod can:

- call the game's own C# APIs and the Unreal Engine APIs that USharp wraps,
- add callbacks to the game's events and delegates,
- register hotkeys for keyboard keys and gamepad buttons,
- change game functions with [Harmony](https://harmony.pardeike.net) hooks.

## Install

1. Download the zip of the [latest release](https://github.com/KintaroModding/b1csharploader/releases/latest).
2. Copy the `b1` folder from the zip into the game folder `BlackMythWukong`.
3. Put each C# mod into its own folder: `CSharpLoader/Mods/<ModName>/<ModName>.dll`, for example `CSharpLoader/Mods/CSharpExample/CSharpExample.dll`.
4. Start the game.

After the install, the game folder looks like this:

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

To remove the loader, delete `version.dll` and the `CSharpLoader` folder.

### Settings

The settings are in `CSharpLoader/b1cs.ini`. `1` turns a setting on, `0` turns it off.

- `Develop`: press Ctrl+F5 in the game to reload all C# mods.
- `Console`: show a console window with the log.
- `EnableJit`: turn on JIT mode, see below.

### JIT mode

To make C# hooks possible, the loader switches the game's Mono runtime from its default interpreter mode to JIT (just-in-time compilation) mode. JIT mode can be faster, but the game developer did not test it, so it can affect the stability of the game. It also stops some trainers from working, for example FLiNG trainers.

Read the instructions of your mods: if none of them needs hooks, you can set `EnableJit=0`.

### Other plugins named version.dll

`version.dll` is a common name for game plugins. If another plugin also uses this name, rename its dll to any other name that ends with `.dll` and put it into `CSharpLoader/Plugins/`. The loader loads these plugins too.

## Projects in this repository

- `CSharpLoaderDll`: the native loader (C++). The game loads it as `version.dll`. It uses the Mono API to start `CSharpManager` after the game has started.
- `CSharpManager`: the mod manager. It loads and manages the C# mods.
- `CSharpModBase`: the API for mods.
- `CSharpModExample`: an example mod.
- `GameDll`: the game's C# dlls. Mod projects reference them.
- `extract_dlls.py`: updates `GameDll` from the game exe.
- `build.ps1`: builds everything and packs the release zip.

### Build

Run `build.ps1` in PowerShell. It builds all projects and packs the release zip into `dist/`:

```
powershell -ExecutionPolicy Bypass -File build.ps1 -Version 1.0.0
```

It needs Visual Studio 2022 (or its Build Tools) with the C++ workload, and the .NET SDK 8 or newer.

### Update GameDll

`GameDll` holds the C# dlls of game version 1.0.21.23831. After a game update, extract them again from the game exe (needs Python 3):

```
python extract_dlls.py --exe "<game folder>/b1/Binaries/Win64/b1-Win64-Shipping.exe"
```

The script updates the dlls in `GameDll` and lists the files that the game no longer contains. Add `--dry-run` to only list them.

## Make a mod

Install the [.NET 8 SDK](https://dotnet.microsoft.com/) or newer to use the latest C# syntax. You can write code with VS Code, Visual Studio or Rider. Start from the example in `CSharpModExample`.

### Entry point

Your mod implements `ICSharpMod`:

```C#
public class MyMod : ICSharpMod
{
    public string Name => "ModExample";
    public string Version => "0.0.1";

    public void Init()
    {
        Console.WriteLine("Init");
    }

    public void DeInit()
    {
        Console.WriteLine("DeInit");
    }
}
```

The manager creates an instance of each class that implements `ICSharpMod` and calls `Init()`.

When you press Ctrl+F5 to reload the mods, the manager calls `DeInit()` of each loaded mod, then loads all mods again. In Develop mode, each reload loads the mod dll as a new assembly, and the old assembly stays in memory. So stop your background threads and remove your event handlers in `DeInit()`. You do not need to remove your key binds: the manager does that.

### Key binds

```C#
Utils.RegisterKeyBind(ModifierKeys.Alt, Key.X, () =>
{

});
```

Gamepad buttons work the same way. When a player holds a button combo, only the bind with the most buttons fires:

```C#
Utils.RegisterGamePadBind(GamePadButton.LeftShoulder | GamePadButton.A, () =>
{

});
```

### C# hooks

See the [Harmony documentation](https://harmony.pardeike.net/articles/patching.html). Hooks need `EnableJit=1`.

Call this in `Init()`:

```C#
var harmony = new Harmony("your name");
var assembly = Assembly.GetExecutingAssembly();
harmony.PatchAll(assembly);

// or implying current assembly:
harmony.PatchAll();
```

Call this in `DeInit()`:

```C#
harmony.UnpatchAll();
```

### Mod dependencies

Put the dlls that your mod depends on into `CSharpLoader/Mods/<ModName>/`, or into `CSharpLoader/Mods/Common/` to share them with all mods.

## Fork changes

Changes compared to the original project:

- `GameDll` updated for game version 1.0.21.23831, with a script to update it after game updates.
- Bug fixes: hotkey repeat, gamepad button combos, error logs, paths with non-English characters, and the input thread at game exit.
- A build script that makes the release zip.

Changes taken from pull requests to the original project:

- "fix: resolved gamepad combination key trigger issue" by reece15
- "Fix: Handle spaces in paths for move command in CSharpManager.csproj" by TheFBplus
- "Add script to extract embedded .NET assemblies from game exe" by inkydragon

Fixes based on issues in the original project:

- "一堆BUG" by bfxh

## License

MIT, see [LICENSE](LICENSE). The dlls in `GameDll` come from the game and belong to its developer. They are only here so that mods can be built.
