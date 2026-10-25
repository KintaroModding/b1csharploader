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
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public delegate void TextFn(string text);

        private static readonly IntPtr module = FindModule();

        /// <summary>Adds a line to the in-game console.</summary>
        public static readonly TextFn? LogLine = GetFunction("CSharpLoader_LogLine");

        /// <summary>Shows a banner on top of the game for 5 seconds, lines are separated by '\n'.</summary>
        public static readonly TextFn? ShowBanner = GetFunction("CSharpLoader_ShowBanner");

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

        private static TextFn? GetFunction(string name)
        {
            IntPtr function = module == IntPtr.Zero ? IntPtr.Zero : GetProcAddress(module, name);
            return function == IntPtr.Zero ? null : (TextFn)Marshal.GetDelegateForFunctionPointer(function, typeof(TextFn));
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandleW(string moduleName);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, BestFitMapping = false)]
        private static extern IntPtr GetProcAddress(IntPtr module, string procName);
    }
}
