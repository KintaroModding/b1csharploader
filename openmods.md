# C# Loader

C# Loader lets Black Myth: Wukong load mods that are written in C#. Much of the game's logic runs in C#, and some of it can only be reached from C#, not from Unreal Engine C++ or Lua.

The loader does nothing on its own. Install it when a mod needs it.

## What it does

- Loads C# mods from the folder `CSharpLoader/Mods`.
- Lets mods use the game's C# code and Unreal Engine.
- Gives mods hotkeys: keyboard keys and gamepad buttons, also button combos.
- Supports Harmony hooks, so mods can change game functions.
- Shows the log in an in-game console: press F10. Scroll with Page Up and Page Down.
- Shows a banner when the loader runs, when it cannot start, and when the game build is newer than the tested one.
- Shows a small red box when there are new errors.
- Writes a log file for each game start into `CSharpLoader/Logs`.
- Reloads all C# mods in the game with Ctrl+F5 (for mod makers).
- Also loads other plugins that are named `version.dll`.

## Screenshots

![C# Loader for Black Myth: Wukong](docs/cover.png)

## How to install

1. Open the latest release on GitHub and download the zip file.
2. Open the game folder `BlackMythWukong`. On Steam: right-click the game, then Manage, then Browse local files.
3. Copy the `b1` folder from the zip into the game folder. You now have `b1/Binaries/Win64/version.dll` and the folder `b1/Binaries/Win64/CSharpLoader`.
4. Put your C# mods into `b1/Binaries/Win64/CSharpLoader/Mods`. Each mod goes into its own folder: `Mods/<ModName>/<ModName>.dll`.
5. Start the game.

To update the loader, copy the `b1` folder of the new release over the old one. Your `Mods` folder stays. If you changed `b1cs.ini`, keep a copy and put it back.

To remove the loader, delete `b1/Binaries/Win64/version.dll` and the folder `b1/Binaries/Win64/CSharpLoader`. The loader does not change your saved games. The mods that you load with it can.

## How to use

The settings are in `b1/Binaries/Win64/CSharpLoader/b1cs.ini`. `1` turns a setting on, `0` turns it off.

- `Console=1`: also shows the log in a separate console window, for troubleshooting. Off by default.
- `Develop=1`: press Ctrl+F5 in the game to reload all C# mods.
- `EnableJit=1`: turns on JIT mode. Mods that use Harmony hooks need it. See Known issues.
- `Overlay=1`: press F10 in the game to open and close an in-game console with the log. Page Up and Page Down scroll it.
- `OverlayKey=F10`: the key for the in-game console. For example `F9`, `Insert`, `Home` or `Tilde`.

When the loader runs, a banner at the top of the game shows its version and the number of loaded mods for 5 seconds. It is red when the loader or a mod failed, and yellow when the game build is newer than the tested one. While the console is closed, a small red box in the top right corner counts new errors.

Do you have a problem? Add the newest file from `b1/Binaries/Win64/CSharpLoader/Logs` to your bug report.

Some mods need helper DLL files. Put them into the mod's own folder, or into `CSharpLoader/Mods/Common` to share them with all mods.

Do you use another plugin that is also named `version.dll`? Rename it to any other name that ends with `.dll`, and put it into `CSharpLoader/Plugins`. The loader then loads it too.

## Compatibility

- Game version: 1.0.21.23831
- Works together with: other `version.dll` plugins, when you put them into `CSharpLoader/Plugins`
- Does not work with: some trainers, for example FLiNG trainers, when `EnableJit=1`

## Known issues

- JIT mode changes how the game runs its C# code. The game developer did not test this mode, so it can affect stability. If you have problems and your mods need no hooks, set `EnableJit=0`.
- Gamepad hotkeys work only with Xbox (XInput) controllers. PlayStation and Switch controllers are not supported.
- A game update can stop the loader until the loader gets an update too.
- The in-game console (F10) may not show in exclusive fullscreen. Use borderless or windowed mode.
- Page Up and Page Down scroll the in-game console, and the game gets these keys too.

## How to build

Anyone can build the release zip from the source code.

1. Install the tools: Visual Studio 2022 (or the Build Tools for Visual Studio 2022) with the "Desktop development with C++" workload, and the .NET SDK 8 or newer. Tested with Visual Studio 2022 Community and the .NET SDK 10.
2. Build it: open PowerShell in the repository folder and run `powershell -ExecutionPolicy Bypass -File build.ps1 -Version 1.0.0`. The script builds `CSharpLoader.sln` with `MSBuild CSharpLoader.sln -restore -p:Configuration=Release -p:Platform=x64` and packs the files.
3. Find the files: the release zip is `dist/B1CSharpLoader-1.0.0.zip`. The same files, not zipped, are in `dist/B1CSharpLoader-1.0.0`.

To make your own C# mods, see the README and the example mod `CSharpModExample` in the repository.

## Credits

- Made by czastack (the original B1CSharpLoader). This version is kept up to date by KintaroModding.
- Fixes and tools from the original project's pull requests and issues by reece15, TheFBplus, inkydragon and bfxh.
- Uses Harmony by Andreas Pardeike, Mono.Cecil and SharpDX (all MIT license).

MIT license: you may use, change and share this loader.
