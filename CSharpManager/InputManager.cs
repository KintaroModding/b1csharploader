using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using CSharpModBase;
using CSharpModBase.Input;

namespace CSharpManager
{
    public class InputManager : IInputManager
    {
        private static readonly Stopwatch Clock = Stopwatch.StartNew();
        private static readonly Func<Key, bool> IsKeyDown = KeyUtils.IsKeyDown;

        // the binds that the current thread registers while a mod's Init runs, see StartRecording
        [ThreadStatic]
        private static List<HotKeyItem>? recordedItems;

        public List<HotKeyItem> BuiltinHotKeyItems { get; } = new();
        public List<HotKeyItem> HotKeyItems { get; } = new();
        public bool EnableGamePad { get; set; } = true;
        public static GamePadButton CurrentGamePadButton { get; set; }

        /// <summary>Runs the action of a hotkey that fires, tests replace it.</summary>
        internal Action<HotKeyItem> Run { get; set; } = RunAction;

        private IntPtr HWnd;

        private void FindMainWindow()
        {
            uint currentProcessId = GetCurrentProcessId();
            StringBuilder stringBuilder = new(64);
            EnumWindows(new EnumWindowsProc((hWnd, lParam) =>
            {
                GetWindowThreadProcessId(hWnd, out uint processId);
                if (processId == currentProcessId &&
                    GetClassName(hWnd, stringBuilder, stringBuilder.Capacity) > 0 &&
                    stringBuilder.ToString() == "UnrealWindow")
                {
                    HWnd = hWnd;
                    return false;
                }
                return true; // continue enumerating
            }), IntPtr.Zero);
        }

        private static void RunAction(HotKeyItem item)
        {
            if (item.RunOnGameThread)
            {
                Utils.TryRunOnGameThread(item.Action);
            }
            else
            {
                Utils.TryRun(item.Action);
            }
        }

        public void Update()
        {
            if (!IsProgramFocused()) return;
            if (EnableGamePad)
            {
                GamePadUtils.GetGamePadButtons(out var buttons);
                CurrentGamePadButton = buttons;
            }
            Dispatch(new InputState(KeyUtils.Modifiers, CurrentGamePadButton, Clock.ElapsedMilliseconds, IsKeyDown));
        }

        /// <summary>
        /// Runs the hotkeys for one input poll. It loops over copies of the lists,
        /// so an action can add and remove binds, or reload the mods.
        /// </summary>
        internal void Dispatch(InputState state)
        {
            HotKeyDispatcher.Handle(Snapshot(BuiltinHotKeyItems), state, Run);
            HotKeyDispatcher.Handle(Snapshot(HotKeyItems), state, Run);
        }

        private static HotKeyItem[] Snapshot(List<HotKeyItem> items)
        {
            lock (items)
            {
                return items.ToArray();
            }
        }

        public void Clear()
        {
            lock (HotKeyItems)
            {
                HotKeyItems.Clear();
            }
        }

        /// <summary>
        /// Starts to collect the binds that this thread registers, until <see cref="StopRecording"/>.
        /// The manager uses it to remove the binds of a mod whose Init failed.
        /// </summary>
        internal List<HotKeyItem> StartRecording()
        {
            recordedItems = new();
            return recordedItems;
        }

        internal void StopRecording()
        {
            recordedItems = null;
        }

        internal void RemoveKeyBinds(ICollection<HotKeyItem> items)
        {
            if (items.Count == 0) return;
            lock (HotKeyItems)
            {
                HotKeyItems.RemoveAll(items.Contains);
            }
        }

        public bool IsProgramFocused()
        {
            if (HWnd == IntPtr.Zero)
            {
                Thread.Sleep(1000);
                FindMainWindow();
            }
            var hwnd = GetForegroundWindow();
            if (hwnd == IntPtr.Zero) return false;
            return hwnd == HWnd;
        }

        [DllImport("user32.dll")]
        private extern static IntPtr GetForegroundWindow();

        [DllImport("user32.dll", SetLastError = true)]
        static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        [DllImport("kernel32.dll")]
        static extern uint GetCurrentProcessId();

        private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        // Win32 API imports
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto, CallingConvention = CallingConvention.StdCall)]
        private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

        public HotKeyItem RegisterBuiltinKeyBind(Key key, Action action)
        {
            return RegisterBuiltinKeyBind(ModifierKeys.None, key, action);
        }

        public HotKeyItem RegisterBuiltinKeyBind(ModifierKeys modifiers, Key key, Action action)
        {
            var item = new HotKeyItem(modifiers, key, action);
            lock (BuiltinHotKeyItems)
            {
                BuiltinHotKeyItems.Add(item);
            }
            return item;
        }

        public void RegisterKeyBind(HotKeyItem item)
        {
            lock (HotKeyItems)
            {
                HotKeyItems.Add(item);
            }
            recordedItems?.Add(item);
        }

        public HotKeyItem RegisterKeyBind(Key key, Action action)
        {
            var item = new HotKeyItem(ModifierKeys.None, key, action);
            RegisterKeyBind(item);
            return item;
        }

        public HotKeyItem RegisterKeyBind(ModifierKeys modifiers, Key key, Action action)
        {
            var item = new HotKeyItem(modifiers, key, action);
            RegisterKeyBind(item);
            return item;
        }

        public HotKeyItem RegisterGamePadBind(GamePadButton button, Action action)
        {
            var item = new HotKeyItem(ModifierKeys.None, Key.None, action)
            {
                GamePadButton = button
            };
            RegisterKeyBind(item);
            return item;
        }
    }
}
