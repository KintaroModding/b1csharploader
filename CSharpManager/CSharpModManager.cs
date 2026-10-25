using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using CSharpModBase;
using CSharpModBase.Input;
using Mono.Cecil;

namespace CSharpManager
{
    public class CSharpModManager
    {
        static string? LoadingModName { get; set; }

        public List<ICSharpMod> LoadedMods { get; } = new();
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
            if (!Directory.Exists(Common.ModDir))
            {
                Log.Error($"Mod dir {Common.ModDir} not exists");
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
                }
                finally
                {
                    LoadingModName = null;
                }
            }
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
        }

        public void ShowStartBanner()
        {
            string version = typeof(CSharpModManager).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "";
            int count = LoadedMods.Count;
            string mods = count == 0 ? "No mods loaded" : count == 1 ? "1 mod loaded" : $"{count} mods loaded";
            NativeLoader.ShowBanner?.Invoke($"C# Loader {version} is running\n{mods}  ·  F10 opens the console");
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
