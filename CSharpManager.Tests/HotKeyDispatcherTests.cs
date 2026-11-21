using System.Collections.Generic;
using System.Linq;
using CSharpManager;
using CSharpModBase.Input;
using Xunit;

namespace CSharpManager.Tests
{
    public class HotKeyDispatcherTests
    {
        private static HotKeyItem Bind(string label, ModifierKeys modifiers, Key key, int repeatMs = 0)
        {
            return new HotKeyItem(label, modifiers, key, () => { }) { RepeatMs = repeatMs };
        }

        private static HotKeyItem PadBind(string label, GamePadButton buttons)
        {
            return new HotKeyItem(label, ModifierKeys.None, Key.None, () => { }) { GamePadButton = buttons };
        }

        // one input poll, returns the labels of the hotkeys that fired
        private static List<string> Poll(HotKeyItem[] items, long nowMs, ModifierKeys modifiers = ModifierKeys.None,
            GamePadButton buttons = GamePadButton.None, params Key[] keysDown)
        {
            var fired = new List<string>();
            var state = new InputState(modifiers, buttons, nowMs, key => keysDown.Contains(key));
            HotKeyDispatcher.Handle(items, state, item => fired.Add(item.Label));
            return fired;
        }

        [Fact]
        public void KeyFiresOncePerPress()
        {
            var items = new[] { Bind("x", ModifierKeys.None, Key.X) };
            Assert.Equal(new[] { "x" }, Poll(items, 1000, keysDown: Key.X));
            Assert.Empty(Poll(items, 1010, keysDown: Key.X));
            Assert.Empty(Poll(items, 5000, keysDown: Key.X));
            Assert.Empty(Poll(items, 5010));
            Assert.Equal(new[] { "x" }, Poll(items, 5020, keysDown: Key.X));
        }

        [Fact]
        public void KeyRepeatsWhileHeld()
        {
            var items = new[] { Bind("x", ModifierKeys.None, Key.X, repeatMs: 100) };
            Assert.Single(Poll(items, 1000, keysDown: Key.X));
            Assert.Empty(Poll(items, 1050, keysDown: Key.X));
            Assert.Single(Poll(items, 1100, keysDown: Key.X));
            Assert.Empty(Poll(items, 1150, keysDown: Key.X));
        }

        [Fact]
        public void ModifiersMustMatchExactly()
        {
            var items = new[] { Bind("ctrl+x", ModifierKeys.Control, Key.X) };
            Assert.Empty(Poll(items, 1000, keysDown: Key.X));
            Assert.Empty(Poll(items, 1010, ModifierKeys.Control | ModifierKeys.Shift, keysDown: Key.X));
            Assert.Single(Poll(items, 1020, ModifierKeys.Control, keysDown: Key.X));
        }

        [Fact]
        public void ModifierAloneCanBeBound()
        {
            var items = new[] { Bind("alt", ModifierKeys.Alt, Key.None) };
            Assert.Empty(Poll(items, 1000));
            Assert.Single(Poll(items, 1010, ModifierKeys.Alt));
        }

        [Fact]
        public void KeyBindIgnoresGamePad()
        {
            var items = new[] { Bind("x", ModifierKeys.None, Key.X) };
            Assert.Empty(Poll(items, 1000, buttons: GamePadButton.A));
        }

        [Fact]
        public void BiggestGamePadComboWins()
        {
            var items = new[]
            {
                PadBind("a", GamePadButton.A),
                PadBind("lb+a", GamePadButton.LeftShoulder | GamePadButton.A),
            };
            Assert.Equal(new[] { "lb+a" }, Poll(items, 1000, buttons: GamePadButton.LeftShoulder | GamePadButton.A));
        }

        [Fact]
        public void ComboPartStaysBlockedUntilReleased()
        {
            var items = new[]
            {
                PadBind("a", GamePadButton.A),
                PadBind("lb+a", GamePadButton.LeftShoulder | GamePadButton.A),
            };
            Assert.Equal(new[] { "lb+a" }, Poll(items, 1000, buttons: GamePadButton.LeftShoulder | GamePadButton.A));
            // LB released first, A is still held: A must not fire
            Assert.Empty(Poll(items, 1010, buttons: GamePadButton.A));
            Assert.Empty(Poll(items, 1020));
            Assert.Equal(new[] { "a" }, Poll(items, 1030, buttons: GamePadButton.A));
        }

        [Fact]
        public void TriggersCountAsButtons()
        {
            var items = new[] { PadBind("lt+y", GamePadButton.LeftTrigger | GamePadButton.Y) };
            Assert.Empty(Poll(items, 1000, buttons: GamePadButton.Y));
            Assert.Single(Poll(items, 1010, buttons: GamePadButton.LeftTrigger | GamePadButton.Y));
        }

        [Fact]
        public void CountBitsCountsButtons()
        {
            Assert.Equal(0, HotKeyDispatcher.CountBits(0));
            Assert.Equal(2, HotKeyDispatcher.CountBits((uint)(GamePadButton.LeftShoulder | GamePadButton.A)));
        }
    }
}
