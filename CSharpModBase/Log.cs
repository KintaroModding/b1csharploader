using System;
using System.Reflection;

namespace CSharpModBase
{
    public static class Log
    {
        private static string DateTimeString => DateTime.Now.ToString("MM-dd HH:mm:ss");  // .fff
        // private static readonly StreamWriter LogFile = File.CreateText("CSharpLog.txt");

        public static void Info(string message)
        {
            string text = $"{DateTimeString} [I] {message}";
            Console.WriteLine(text);
            // LogFile.WriteLine(text);
        }

        public static void Debug(string message)
        {
            using (var color = new ChangeConsoleColor(ConsoleColor.Gray))
            {
                string text = $"{DateTimeString} [D] {message}";
                Console.WriteLine(text);
                // LogFile.WriteLine(text);
            }
        }

        public static void Warn(string message)
        {
            using (var color = new ChangeConsoleColor(ConsoleColor.Yellow))
            {
                string text = $"{DateTimeString} [W] {message}";
                Console.WriteLine(text);
                // LogFile.WriteLine(text);
            }
        }

        public static void WarnIf(bool condition, string message)
        {
            if (condition) {
                Warn(message);
            }
        }

        public static void Error(string message)
        {
            using (var color = new ChangeConsoleColor(ConsoleColor.Red))
            {
                string text = $"{DateTimeString} [E] {message}";
                Console.Error.WriteLine(text);
                // LogFile.WriteLine(text);
            }
        }

        public static void Error(Exception e)
        {
            // ToString includes type, message, inner exceptions and stack trace
            Error(e.ToString());
            if (e is ReflectionTypeLoadException typeLoadException)
            {
                foreach (var loaderException in typeLoadException.LoaderExceptions)
                {
                    if (loaderException != null)
                    {
                        Error($"LoaderException: {loaderException}");
                    }
                }
            }
        }
    }


    public readonly struct ChangeConsoleColor : IDisposable
    {
        readonly ConsoleColor currentForeground;

        public ChangeConsoleColor(ConsoleColor color)
        {
            currentForeground = Console.ForegroundColor;
            Console.ForegroundColor = color;
        }

        public void Dispose()
        {
            Console.ForegroundColor = currentForeground;
        }
    }
}
