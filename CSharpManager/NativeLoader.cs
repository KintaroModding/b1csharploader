using System;
using System.IO;
using System.Runtime.InteropServices;

namespace CSharpManager
{
    /// <summary>
    /// Functions of the native loader, which is loaded from the game folder as version.dll or hid.dll.
    /// Each one is null when the loader does not have it.
    /// </summary>
    internal static class NativeLoader
    {
        public const int BannerInfo = 0;
        public const int BannerWarning = 1;
        public const int BannerError = 2;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public delegate void TextFn(string text);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public delegate void BannerFn(string text, int level);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr GetTextFn();

        private static readonly IntPtr module = FindModule();

        /// <summary>Adds a line to the log file and the in-game console.</summary>
        public static readonly TextFn? LogLine = GetFunction<TextFn>("CSharpLoader_LogLine");

        /// <summary>Shows a banner on top of the game for 5 seconds, lines are separated by '\n'.</summary>
        public static readonly BannerFn? ShowBanner = GetFunction<BannerFn>("CSharpLoader_ShowBanner");

        /// <summary>Sets the lines under the in-game console's header, lines that start with '!' show in red.</summary>
        public static readonly TextFn? SetConsoleInfo = GetFunction<TextFn>("CSharpLoader_SetConsoleInfo");

        /// <summary>The key that opens the in-game console, for example "F10".</summary>
        public static readonly string OverlayKey = GetOverlayKey();

        private static IntPtr FindModule()
        {
            foreach (string name in new[] { "version.dll", "hid.dll" })
            {
                IntPtr handle = GetModuleHandleW(Path.GetFullPath(name));
                if (handle != IntPtr.Zero && GetProcAddress(handle, "CSharpLoader_LogLine") != IntPtr.Zero)
                {
                    return handle;
                }
            }
            return IntPtr.Zero;
        }

        private static T? GetFunction<T>(string name) where T : Delegate
        {
            IntPtr function = module == IntPtr.Zero ? IntPtr.Zero : GetProcAddress(module, name);
            return function == IntPtr.Zero ? null : (T)Marshal.GetDelegateForFunctionPointer(function, typeof(T));
        }

        private static string GetOverlayKey()
        {
            IntPtr text = GetFunction<GetTextFn>("CSharpLoader_GetOverlayKey")?.Invoke() ?? IntPtr.Zero;
            return text == IntPtr.Zero ? "F10" : Marshal.PtrToStringUni(text) ?? "F10";
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandleW(string moduleName);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, BestFitMapping = false)]
        private static extern IntPtr GetProcAddress(IntPtr module, string procName);
    }
}
