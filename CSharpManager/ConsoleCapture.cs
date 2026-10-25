using System;
using System.IO;
using System.Text;

namespace CSharpManager
{
    /// <summary>
    /// Copies every line written to Console.Out and Console.Error to the native loader,
    /// which shows the lines in the in-game console (F10).
    /// </summary>
    internal sealed class ConsoleCapture : TextWriter
    {
        private readonly TextWriter inner;
        private readonly StringBuilder pending = new();

        private ConsoleCapture(TextWriter inner)
        {
            this.inner = inner;
        }

        public override Encoding Encoding => inner.Encoding;

        public static void Install()
        {
            if (NativeLoader.LogLine == null)
            {
                return;
            }
            Console.SetOut(Synchronized(new ConsoleCapture(Console.Out)));
            Console.SetError(Synchronized(new ConsoleCapture(Console.Error)));
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
                NativeLoader.LogLine?.Invoke(pending.ToString());
                pending.Clear();
            }
            else if (c != '\r')
            {
                pending.Append(c);
            }
        }
    }
}
