using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using CSharpModBase;
using CSharpModBase.Input;
using HarmonyLib;
using Mono.Cecil;
using Mono.Cecil.Cil;

namespace CSharpManager
{
    public class CSharpModManager
    {
        // the game build this loader version is tested with, the same as GameDll (b1.BuildEnv.BuildVersion)
        private const string TestedGameBuild = "23831";
        private static readonly string Separator = "  " + (char)0xB7 + "  ";

        // the mod folder whose dll this thread loads right now
        [ThreadStatic]
        private static string? loadingModName;

        // the mod folder of each assembly that came from a mod folder, to find its dependencies later
        private static readonly Dictionary<Assembly, string> ModOfAssembly = new();

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

        private static Assembly? TryLoadDll(string path, string? modName)
        {
            if (!File.Exists(path))
            {
                return null;
            }
            Assembly assembly = Assembly.LoadFrom(path);
            if (modName != null)
            {
                RememberMod(assembly, modName);
            }
            return assembly;
        }

        private static void RememberMod(Assembly assembly, string modName)
        {
            lock (ModOfAssembly)
            {
                ModOfAssembly[assembly] = modName;
            }
        }

        private static string? ModNameOf(Assembly? assembly)
        {
            if (assembly == null) return null;
            lock (ModOfAssembly)
            {
                return ModOfAssembly.TryGetValue(assembly, out string? modName) ? modName : null;
            }
        }

        /// <summary>
        /// Returns an assembly that is loaded already and is not older than the requested one.
        /// So all mods use the same CSharpModBase and 0Harmony, even when a mod folder has its own copy.
        /// </summary>
        internal static Assembly? FindLoadedAssembly(AssemblyName name)
        {
            foreach (Assembly loaded in AppDomain.CurrentDomain.GetAssemblies())
            {
                AssemblyName loadedName = loaded.GetName();
                if (string.Equals(loadedName.Name, name.Name, StringComparison.OrdinalIgnoreCase) &&
                    (name.Version == null || (loadedName.Version != null && loadedName.Version >= name.Version)))
                {
                    return loaded;
                }
            }
            return null;
        }

        private static Assembly? AssemblyResolve(object sender, ResolveEventArgs args)
        {
            try
            {
                AssemblyName name = new(args.Name);
                Assembly? loaded = FindLoadedAssembly(name);
                if (loaded != null)
                {
                    return loaded;
                }
                string dllName = $"{name.Name}.dll";
                // a mod can also need a dependency after Init, for example in a hotkey
                string? modName = loadingModName ?? ModNameOf(args.RequestingAssembly);
                if (modName != null)
                {
                    Assembly? fromMod = TryLoadDll(Path.Combine(Common.ModPath, modName, dllName), modName);
                    if (fromMod != null)
                    {
                        return fromMod;
                    }
                }
                Assembly? shared = TryLoadDll(Path.Combine(Common.ModPath, "Common", dllName), null) ??
                                   TryLoadDll(Path.Combine(Common.LoaderPath, dllName), null);
                if (shared != null || modName != null || !Directory.Exists(Common.ModPath))
                {
                    return shared;
                }
                // the requesting mod is not known, look into each mod folder
                foreach (string dir in Directory.GetDirectories(Common.ModPath))
                {
                    Assembly? found = TryLoadDll(Path.Combine(dir, dllName), Path.GetFileName(dir));
                    if (found != null)
                    {
                        return found;
                    }
                }
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
            Ini iniFile = new(Path.Combine(Common.LoaderPath, "b1cs.ini"));
            Develop = iniFile.GetValue("Develop", "Settings", "0").Trim() == "1";
            Log.Debug($"Develop: {Develop}");
        }

        public void LoadMods()
        {
            LoadedMods.Clear();
            FailedMods.Clear();
            if (!Directory.Exists(Common.ModPath))
            {
                Log.Error($"Mod dir {Common.ModPath} not exists");
                UpdateConsoleInfo();
                return;
            }
            string[] dirs = Directory.GetDirectories(Common.ModPath);
            Array.Sort(dirs, StringComparer.OrdinalIgnoreCase);
            foreach (var dir in dirs)
            {
                string modName = Path.GetFileName(dir);
                string dllPath = Path.Combine(dir, $"{modName}.dll");
                if (!File.Exists(dllPath)) continue;
                loadingModName = modName;
                try
                {
                    Log.Debug($"======== Loading {dllPath} ========");
                    Assembly assembly = Develop ? LoadAsNewAssembly(dllPath) : Assembly.LoadFrom(dllPath);
                    RememberMod(assembly, modName);
                    // abstract base classes cannot be created
                    Type[] modTypes = assembly.GetTypes()
                        .Where(type => typeof(ICSharpMod).IsAssignableFrom(type) && !type.IsAbstract && !type.IsInterface)
                        .ToArray();
                    if (modTypes.Length == 0)
                    {
                        Log.Warn($"{dllPath} has no class that implements ICSharpMod, so it does nothing. " +
                                 "Is CSharpModBase.dll the one of this loader version?");
                    }
                    bool failed = false;
                    foreach (Type type in modTypes)
                    {
                        failed |= !InitMod(type);
                    }
                    if (failed)
                    {
                        FailedMods.Add(modName);
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
                    loadingModName = null;
                }
            }
            UpdateConsoleInfo();
        }

        /// <summary>Creates the mod and calls Init. When Init fails, removes the key binds and the Harmony patches that it added.</summary>
        private bool InitMod(Type type)
        {
            Log.Debug($"Found ICSharpMod: {type}");
            HashSet<string> ownersBefore = HarmonyPatchOwners();
            List<HotKeyItem> binds = InputManager.StartRecording();
            try
            {
                if (Activator.CreateInstance(type) is ICSharpMod mod)
                {
                    mod.Init();
                    LoadedMods.Add(mod);
                    Log.Debug($"Loaded mod {mod.Name} {mod.Version}");
                }
                return true;
            }
            catch (Exception e)
            {
                Log.Error($"Init of {type} failed:");
                Log.Error(e);
                // nothing of a failed mod stays active
                InputManager.RemoveKeyBinds(binds);
                int owners = RemoveNewHarmonyPatches(ownersBefore);
                if (binds.Count > 0 || owners > 0)
                {
                    Log.Warn($"Removed {binds.Count} key binds and the Harmony patches of {owners} Harmony ids of {type}");
                }
                return false;
            }
            finally
            {
                InputManager.StopRecording();
            }
        }

        private static bool IsHarmonyLoaded()
        {
            return AppDomain.CurrentDomain.GetAssemblies().Any(a => a.GetName().Name == "0Harmony");
        }

        // only touches Harmony when a mod has loaded it already, so mods without hooks never load it
        private static HashSet<string> HarmonyPatchOwners()
        {
            if (!IsHarmonyLoaded())
            {
                return new HashSet<string>();
            }
            try
            {
                return GetHarmonyPatchOwners();
            }
            catch (Exception e)
            {
                Log.Warn($"Could not list the Harmony patches: {e.Message}");
                return new HashSet<string>();
            }
        }

        // a separate method, so 0Harmony.dll is only loaded when it is called
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static HashSet<string> GetHarmonyPatchOwners()
        {
            var owners = new HashSet<string>();
            foreach (MethodBase method in Harmony.GetAllPatchedMethods())
            {
                Patches? info = Harmony.GetPatchInfo(method);
                if (info != null)
                {
                    owners.UnionWith(info.Owners);
                }
            }
            return owners;
        }

        private static int RemoveNewHarmonyPatches(HashSet<string> ownersBefore)
        {
            int removed = 0;
            foreach (string owner in HarmonyPatchOwners())
            {
                if (ownersBefore.Contains(owner)) continue;
                try
                {
                    UnpatchAll(owner);
                    removed++;
                }
                catch (Exception e)
                {
                    Log.Error($"Could not remove the Harmony patches of {owner}:");
                    Log.Error(e);
                }
            }
            return removed;
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private static void UnpatchAll(string owner)
        {
            // the id must be given, UnpatchAll(null) removes the patches of all mods
            new Harmony(owner).UnpatchAll(owner);
        }

        /// <summary>
        /// Develop mode: loads the dll under a new assembly name, so a reload uses the rebuilt dll.
        /// The pdb next to it is loaded too, so stack traces can show line numbers.
        /// </summary>
        private static Assembly LoadAsNewAssembly(string dllPath)
        {
            if (File.Exists(Path.ChangeExtension(dllPath, ".pdb")))
            {
                try
                {
                    return LoadAsNewAssembly(dllPath, true);
                }
                catch (Exception e)
                {
                    Log.Warn($"Could not load the pdb of {dllPath}, stack traces have no line numbers: {e.Message}");
                }
            }
            return LoadAsNewAssembly(dllPath, false);
        }

        internal static Assembly LoadAsNewAssembly(string dllPath, bool withSymbols)
        {
            using var assemblyDef = AssemblyDefinition.ReadAssembly(dllPath, new ReaderParameters { ReadSymbols = withSymbols });
            assemblyDef.Name.Name += DateTime.Now.ToString("_yyyyMMdd_HHmmssffff");
            using MemoryStream image = new();
            if (!withSymbols)
            {
                assemblyDef.Write(image);
                return Assembly.Load(image.ToArray());
            }
            using MemoryStream symbols = new();
            assemblyDef.Write(image, new WriterParameters
            {
                WriteSymbols = true,
                SymbolStream = symbols,
                SymbolWriterProvider = new PortablePdbWriterProvider(),
            });
            return Assembly.Load(image.ToArray(), symbols.ToArray());
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
            HotKeyItem reload = InputManager.RegisterBuiltinKeyBind(ModifierKeys.Control, Key.F5, ReloadMods);
            // the first load does not run on the game thread either, so Init and DeInit always run on a background thread
            reload.RunOnGameThread = false;
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
                try
                {
                    InputManager.Update();
                }
                catch (Exception e)
                {
                    // keep the hotkeys working, an exception here would end the thread
                    Log.Error("Input loop failed:");
                    Log.Error(e);
                    Thread.Sleep(1000);
                }
                Thread.Sleep(10);  // 10ms
            }
        }
    }
}
