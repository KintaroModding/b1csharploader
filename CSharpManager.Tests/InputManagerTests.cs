using System.Linq;
using System.Threading;
using CSharpManager;
using CSharpModBase.Input;
using Xunit;

namespace CSharpManager.Tests
{
    public class InputManagerTests
    {
        private static InputManager CreateManager()
        {
            // run the actions right away, without the game thread
            return new InputManager { Run = item => item.Action() };
        }

        private static InputState Pressed(long nowMs, params Key[] keys)
        {
            return new InputState(ModifierKeys.None, GamePadButton.None, nowMs, key => keys.Contains(key));
        }

        [Fact]
        public void ActionCanRegisterBindsWhilePolling()
        {
            var manager = CreateManager();
            manager.RegisterKeyBind(Key.X, () => manager.RegisterKeyBind(Key.Y, () => { }));
            manager.RegisterKeyBind(Key.Z, () => { });

            manager.Dispatch(Pressed(1000, Key.X));

            Assert.Equal(3, manager.HotKeyItems.Count);
        }

        [Fact]
        public void ActionCanClearBindsWhilePolling()
        {
            var manager = CreateManager();
            int zCount = 0;
            manager.RegisterKeyBind(Key.X, manager.Clear);
            manager.RegisterKeyBind(Key.Z, () => zCount++);

            manager.Dispatch(Pressed(1000, Key.X, Key.Z));

            Assert.Empty(manager.HotKeyItems);
            // Z was in the copy of this poll
            Assert.Equal(1, zCount);
        }

        [Fact]
        public void BuiltinActionCanReplaceModBinds()
        {
            var manager = CreateManager();
            manager.RegisterKeyBind(Key.Y, () => { });
            // like Ctrl+F5: removes the binds of the mods, then the mods register new ones
            manager.RegisterBuiltinKeyBind(Key.F5, () =>
            {
                manager.Clear();
                manager.RegisterKeyBind(Key.Y, () => { });
            });

            manager.Dispatch(Pressed(1000, Key.F5));

            Assert.Single(manager.HotKeyItems);
        }

        [Fact]
        public void RemovesOnlyRecordedBinds()
        {
            var manager = CreateManager();
            HotKeyItem before = manager.RegisterKeyBind(Key.A, () => { });
            var recorded = manager.StartRecording();
            manager.RegisterKeyBind(Key.B, () => { });
            manager.RegisterGamePadBind(GamePadButton.A, () => { });
            manager.StopRecording();
            HotKeyItem after = manager.RegisterKeyBind(Key.C, () => { });

            manager.RemoveKeyBinds(recorded);

            Assert.Equal(new[] { before, after }, manager.HotKeyItems);
        }

        [Fact]
        public void RecordingIsPerThread()
        {
            var manager = CreateManager();
            var recorded = manager.StartRecording();
            var other = new Thread(() => manager.RegisterKeyBind(Key.B, () => { }));
            other.Start();
            other.Join();
            manager.StopRecording();

            Assert.Empty(recorded);
            Assert.Single(manager.HotKeyItems);
        }
    }
}
