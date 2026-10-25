using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace CSharpManager
{
    /// <summary>
    /// Copies every line written to Console.Out and Console.Error to the native loader,
    /// which shows the lines in the in-game console (F10).
    /// </summary>
    internal sealed class ConsoleCapture : TextWriter
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        private delegate void LogLineFn(string line);

        private static LogLineFn? logLine;
        private readonly TextWriter inner;
        private readonly StringBuilder pending = new();

        private ConsoleCapture(TextWriter inner)
        {
            this.inner = inner;
        }

        public override Encoding Encoding => inner.Encoding;

        public static void Install()
        {
            logLine = FindLogLine();
            if (logLine == null)
            {
                return;
            }
            Console.SetOut(Synchronized(new ConsoleCapture(Console.Out)));
            Console.SetError(Synchronized(new ConsoleCapture(Console.Error)));
        }

        private static LogLineFn? FindLogLine()
        {
            // the native loader is loaded from the game folder as version.dll or hid.dll
            foreach (string name in new[] { "version.dll", "hid.dll" })
            {
                IntPtr module = GetModuleHandleW(Path.GetFullPath(name));
                if (module == IntPtr.Zero)
                {
                    continue;
                }
                IntPtr function = GetProcAddress(module, "CSharpLoader_LogLine");
                if (function != IntPtr.Zero)
                {
                    return (LogLineFn)Marshal.GetDelegateForFunctionPointer(function, typeof(LogLineFn));
                }
            }
            return null;
        }

        public override void Write(char value)
        {
            inner.Write(value);
            Capture(value);
        }

        public override void Write(char[] buffer, int index, int count)
        {
            inner.Write(buffer, index, count);
            for (int i = index; i < index + count; i++)
            {
                Capture(buffer[i]);
            }
        }

        public override void Write(string? value)
        {
            if (value == null)
            {
                return;
            }
            inner.Write(value);
            foreach (char c in value)
            {
                Capture(c);
            }
        }

        public override void Flush()
        {
            inner.Flush();
        }

        private void Capture(char c)
        {
            if (c == '\n')
            {
                logLine?.Invoke(pending.ToString());
                pending.Clear();
            }
            else if (c != '\r')
            {
                pending.Append(c);
            }
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandleW(string moduleName);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, BestFitMapping = false)]
        private static extern IntPtr GetProcAddress(IntPtr module, string procName);
    }
}
