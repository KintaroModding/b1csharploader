using System;
using System.Diagnostics;
using System.Threading;
using SharpDX.XInput;

namespace CSharpModBase.Input
{
    [Flags]
    public enum GamePadButton
    {
        None = GamepadButtonFlags.None,
        DPadUp = GamepadButtonFlags.DPadUp,
        DPadDown = GamepadButtonFlags.DPadDown,
        DPadLeft = GamepadButtonFlags.DPadLeft,
        DPadRight = GamepadButtonFlags.DPadRight,
        Start = GamepadButtonFlags.Start,
        Back = GamepadButtonFlags.Back,
        LeftThumb = GamepadButtonFlags.LeftThumb,
        RightThumb = GamepadButtonFlags.RightThumb,
        LeftShoulder = GamepadButtonFlags.LeftShoulder,
        RightShoulder = GamepadButtonFlags.RightShoulder,
        A = GamepadButtonFlags.A,
        B = GamepadButtonFlags.B,
        X = GamepadButtonFlags.X,
        Y = GamepadButtonFlags.Y,
        LeftTrigger = 0x10000,
        RightTrigger = 0x20000,
        DPadRightUp = DPadRight | DPadUp,
        DPadLeftDown = DPadLeft | DPadDown,
        DPadRightDown = DPadRight | DPadDown,
        DPadLeftUp = DPadLeft | DPadUp,
    }


    public static class GamePadUtils
    {
        // XInput is slow for a controller that is not connected, so look for one only this often
        private const long RetryMs = 1000;

        private static readonly Controller controller = new Controller(UserIndex.One);
        private static readonly Stopwatch clock = Stopwatch.StartNew();
        private static long nextTryMs;

        public static bool GetGamePadButtons(out GamePadButton flags)
        {
            flags = GamePadButton.None;
            long now = clock.ElapsedMilliseconds;
            if (now < Interlocked.Read(ref nextTryMs))
            {
                return false;
            }
            if (controller.GetState(out State state))
            {
                Gamepad gamepad = state.Gamepad;
                flags = (GamePadButton)gamepad.Buttons;
                if (gamepad.LeftTrigger > 100) flags |= GamePadButton.LeftTrigger;
                if (gamepad.RightTrigger > 100) flags |= GamePadButton.RightTrigger;
                return true;
            }
            Interlocked.Exchange(ref nextTryMs, now + RetryMs);
            return false;
        }
    }


    public class GamePadButtonEvent : EventArgs
    {
        public GamePadButton Button { get; set; }
    }


    public delegate void GamePadButtonEventHandler(GamePadButtonEvent e);
}
