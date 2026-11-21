using System;
using System.Collections.Generic;
using CSharpModBase.Input;

namespace CSharpManager
{
    /// <summary>The keyboard and gamepad state of one input poll.</summary>
    internal readonly struct InputState
    {
        public InputState(ModifierKeys modifiers, GamePadButton buttons, long nowMs, Func<Key, bool> isKeyDown)
        {
            Modifiers = modifiers;
            Buttons = buttons;
            NowMs = nowMs;
            IsKeyDown = isKeyDown;
        }

        public ModifierKeys Modifiers { get; }
        public GamePadButton Buttons { get; }
        /// <summary>Milliseconds of a clock that only goes forward.</summary>
        public long NowMs { get; }
        public Func<Key, bool> IsKeyDown { get; }
    }

    /// <summary>
    /// Decides which hotkeys fire for one input poll. It makes no Windows or game calls, so tests can run it.
    /// </summary>
    internal static class HotKeyDispatcher
    {
        /// <param name="run">runs the action of a hotkey that fires</param>
        public static void Handle(IReadOnlyList<HotKeyItem> items, InputState state, Action<HotKeyItem> run)
        {
            long now = state.NowMs;
            ModifierKeys modifiers = state.Modifiers;
            GamePadButton buttons = state.Buttons;

            // the biggest held gamepad combo wins, combos inside it are blocked
            int maxButtonCount = 0;
            foreach (var item in items)
            {
                if (IsGamePadMatch(item, buttons))
                {
                    maxButtonCount = Math.Max(maxButtonCount, CountBits((uint)item.GamePadButton));
                }
            }

            foreach (var item in items)
            {
                // Key.None with modifiers binds the modifier keys alone
                bool keyMatch = item.Modifiers == modifiers &&
                    (item.Key == Key.None ? modifiers != ModifierKeys.None : state.IsKeyDown(item.Key));
                bool gamePadMatch = IsGamePadMatch(item, buttons);
                if (!keyMatch && gamePadMatch && CountBits((uint)item.GamePadButton) < maxButtonCount)
                {
                    // part of a bigger held combo, keep it blocked until it is released
                    item.IsPressed = true;
                    item.LastTriggerMs = now;
                }
                else if (keyMatch || gamePadMatch)
                {
                    if (item.IsPressed && (item.RepeatMs == 0 || now - item.LastTriggerMs < item.RepeatMs))
                    {
                        continue;
                    }
                    item.IsPressed = true;
                    item.LastTriggerMs = now;
                    run(item);
                }
                else if (item.IsPressed)
                {
                    item.IsPressed = false;
                }
            }
        }

        private static bool IsGamePadMatch(HotKeyItem item, GamePadButton buttons)
        {
            return item.GamePadButton != GamePadButton.None && (buttons & item.GamePadButton) == item.GamePadButton;
        }

        internal static int CountBits(uint value)
        {
            int count = 0;
            while (value != 0)
            {
                value &= value - 1;
                count++;
            }
            return count;
        }
    }
}
