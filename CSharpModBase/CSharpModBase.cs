using System;
using System.IO;

namespace CSharpModBase
{
    public static class Common
    {
        /// <summary>Relative to the game's working folder, use <see cref="LoaderPath"/> instead.</summary>
        public const string LoaderDir = "CSharpLoader";
        /// <summary>Relative to the game's working folder, use <see cref="ModPath"/> instead.</summary>
        public const string ModDir = "CSharpLoader\\Mods";
        /// <summary>Relative to the game's working folder, use <see cref="DataPath"/> instead.</summary>
        public const string DataDir = "CSharpLoader\\Data";

        /// <summary>Full path of the CSharpLoader folder.</summary>
        public static readonly string LoaderPath = FindLoaderPath();
        /// <summary>Full path of the CSharpLoader\Mods folder.</summary>
        public static readonly string ModPath = Path.Combine(LoaderPath, "Mods");
        /// <summary>Full path of the CSharpLoader\Data folder.</summary>
        public static readonly string DataPath = Path.Combine(LoaderPath, "Data");

        // CSharpModBase.dll is in the CSharpLoader folder, so the paths do not depend on the working folder
        private static string FindLoaderPath()
        {
            try
            {
                string? dir = Path.GetDirectoryName(typeof(Common).Assembly.Location);
                if (!string.IsNullOrEmpty(dir))
                {
                    return dir!;
                }
            }
            catch (Exception)
            {
                // Location can fail for assemblies loaded from bytes
            }
            return Path.GetFullPath(LoaderDir);
        }
    }


    public interface ICSharpMod
    {
        /// <summary>
        /// mod name
        /// </summary>
        string Name { get; }

        /// <summary>
        /// mod version
        /// </summary>
        string Version { get; }

        /// <summary>
        /// called when the mod is loaded, on a background thread, not on the game thread
        /// </summary>
        void Init();
        /// <summary>
        /// called before the manager reloads the mods, on a background thread, not on the game thread
        /// </summary>
        void DeInit();
    }
}
