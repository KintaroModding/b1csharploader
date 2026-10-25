using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using CSharpModBase;
using CSharpModBase.Input;
using Mono.Cecil;

namespace CSharpManager
{
    public class CSharpModManager
    {
        // the game build this loader version is tested with, the same as GameDll (b1.BuildEnv.BuildVersion)
        private const string TestedGameBuild = "23831";
        private static readonly string Separator = "  " + (char)0xB7 + "  ";

        static string? LoadingModName { get; set; }

        public List<ICSharpMod> LoadedMods { get; } = new();
        /// <summary>Folder names of the mods that failed to load.</summary>
        public List<string> FailedMods { get; } = new();
        public InputManager InputManager { get; } = new();
        public bool Develop { get; set; }
        private Thread? loopThread;

        static CSharpModManager()
        {
            // first, so the in-game console also gets the lines below
            ConsoleCapture.Install();

            AppDomain currentDomain = AppDomain.CurrentDomain;
            currentDomain.AssemblyResolve += AssemblyResolve;
            currentDomain.UnhandledException += OnUnhandledException;

            TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;
        }

        private static Assembly? TryLoadDll(string path)
        {
            if (File.Exists(path))
            {
                return Assembly.LoadFrom(path);
            }
            return null;
        }

        private static Assembly? AssemblyResolve(object sender, ResolveEventArgs args)
        {
            try
            {
                if (LoadingModName == null)
                {
                    return null;
                }
                string dllName = $"{new AssemblyName(args.Name).Name}.dll";
                return TryLoadDll(Path.Combine(Common.ModDir, LoadingModName, dllName)) ??
                       TryLoadDll(Path.Combine(Common.ModDir, "Common", dllName)) ??
                       TryLoadDll(Path.Combine(Common.LoaderDir, dllName));
            }
            catch (Exception e)
            {
                Log.Error($"Load assembly {args.Name} failed:");
                Log.Error(e);
            }
            // Assembly.Load here would raise this event again and recurse
            return null;
        }

        private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            Log.Error($"UnhandledException:");
            Log.Error((Exception)e.ExceptionObject);
        }

        private static void OnUnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
        {
            Log.Error($"UnobservedTaskException:");
            Log.Error(e.Exception);
        }

        public CSharpModManager()
        {
            Utils.InitInputManager(InputManager);
            // load config from ini
            Ini iniFile = new(Path.Combine(Common.LoaderDir, "b1cs.ini"));
            Develop = iniFile.GetValue("Develop", "Settings", "1").Trim() == "1";
            Log.Debug($"Develop: {Develop}");
        }

        public void LoadMods()
        {
            LoadedMods.Clear();
            FailedMods.Clear();
            if (!Directory.Exists(Common.ModDir))
            {
                Log.Error($"Mod dir {Common.ModDir} not exists");
                UpdateConsoleInfo();
                return;
            }
            string[] dirs = Directory.GetDirectories(Common.ModDir);
            Type modInterface = typeof(ICSharpMod);
            foreach (var dir in dirs)
            {
                string modName = Path.GetFileName(dir);
                string dllPath = Path.Combine(dir, $"{modName}.dll");
                if (!File.Exists(dllPath)) continue;
                LoadingModName = modName;
                try
                {
                    Log.Debug($"======== Loading {dllPath} ========");
                    Assembly assembly;
                    if (Develop)
                    {
                        using var assemblyDef = AssemblyDefinition.ReadAssembly(dllPath);
                        assemblyDef.Name.Name += DateTime.Now.ToString("_yyyyMMdd_HHmmssffff");
                        using MemoryStream stream = new();
                        assemblyDef.Write(stream);
                        assembly = Assembly.Load(stream.ToArray());
                    }
                    else
                    {
                        assembly = Assembly.LoadFrom(dllPath);
                    }
                    foreach (Type type in assembly.GetTypes())
                    {
                        // abstract base classes cannot be created
                        if (modInterface.IsAssignableFrom(type) && !type.IsAbstract && !type.IsInterface)
                        {
                            Log.Debug($"Found ICSharpMod: {type}");

                            if (Activator.CreateInstance(type) is ICSharpMod mod)
                            {
                                mod.Init();
                                LoadedMods.Add(mod);
                                Log.Debug($"Loaded mod {mod.Name} {mod.Version}");
                            }
                        }
                    }
                }
                catch (Exception e)
                {
                    Log.Error($"Load {dllPath} failed:");
                    Log.Error(e);
                    FailedMods.Add(modName);
                }
                finally
                {
                    LoadingModName = null;
                }
            }
            UpdateConsoleInfo();
        }

        public void ReloadMods()
        {
            Log.Debug("ReloadMods");
            InputManager.Clear();
            foreach (var mod in LoadedMods)
            {
                try
                {
                    mod.DeInit();
                }
                catch (Exception e)
                {
                    Log.Error($"DeInit {mod.Name} failed:");
                    Log.Error(e);
                }
            }
            LoadMods();
            NativeLoader.ShowBanner?.Invoke($"Mods reloaded\n{ModSummary()}", ModsLevel());
        }

        public void ShowStartBanner()
        {
            string version = typeof(CSharpModManager).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "";
            var text = new StringBuilder($"C# Loader {version} is running\n{ModSummary()}{Separator}{NativeLoader.OverlayKey} opens the console");
            int level = ModsLevel();
            string? gameWarning = CheckGameBuild();
            if (gameWarning != null)
            {
                text.Append('\n').Append(gameWarning);
                level = Math.Max(level, NativeLoader.BannerWarning);
            }
            NativeLoader.ShowBanner?.Invoke(text.ToString(), level);
        }

        private string ModSummary()
        {
            int count = LoadedMods.Count;
            string summary = count == 0 ? "No mods loaded" : count == 1 ? "1 mod loaded" : $"{count} mods loaded";
            return FailedMods.Count == 0 ? summary : $"{summary}{Separator}{FailedMods.Count} failed";
        }

        private int ModsLevel()
        {
            return FailedMods.Count == 0 ? NativeLoader.BannerInfo : NativeLoader.BannerError;
        }

        // the mod list under the in-game console's header
        private void UpdateConsoleInfo()
        {
            var info = new StringBuilder(LoadedMods.Count == 0
                ? "Mods: none"
                : $"Mods ({LoadedMods.Count}): " + string.Join(Separator, LoadedMods.Select(mod => $"{mod.Name} {mod.Version}")));
            if (FailedMods.Count > 0)
            {
                // '!' shows the line in red
                info.Append($"\n!Failed ({FailedMods.Count}): ").Append(string.Join(Separator, FailedMods));
            }
            NativeLoader.SetConsoleInfo?.Invoke(info.ToString());
        }

        /// <summary>Returns a warning when the game build is not the tested one, null when it is or is unknown.</summary>
        private static string? CheckGameBuild()
        {
            string? build = GetGameBuild();
            if (build == null || build == TestedGameBuild)
            {
                return null;
            }
            bool newer = int.TryParse(build, out int current) && int.TryParse(TestedGameBuild, out int tested) && current > tested;
            string warning = $"Game build {build} is {(newer ? "newer" : "older")} than the tested build {TestedGameBuild}";
            Log.Warn($"{warning}. Mods can stop working until C# Loader is updated for this game version.");
            return warning;
        }

        private static string? GetGameBuild()
        {
            try
            {
                // b1.BuildEnv in the game's BtlSvr.Main holds the build number, for example "23831"
                Assembly game = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(a => a.GetName().Name == "BtlSvr.Main")
                    ?? Assembly.Load("BtlSvr.Main");
                FieldInfo? field = game.GetType("b1.BuildEnv")?.GetField("BuildVersion", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
                return field?.GetValue(null) as string;
            }
            catch (Exception e)
            {
                Log.Warn($"Could not read the game build: {e.Message}");
                return null;
            }
        }

        public void StartLoop()
        {
            InputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, ReloadMods);
            loopThread = new Thread(Loop)
            {
                // a foreground thread can keep the game process from exiting
                IsBackground = true,
            };
            loopThread.Start();
        }

        private void Loop()
        {
            while (true)
            {
                InputManager.Update();
                Thread.Sleep(10);  // 10ms
            }
        }
    }
}
