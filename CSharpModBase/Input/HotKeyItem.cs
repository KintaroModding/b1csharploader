using System;

namespace CSharpModBase.Input
{
    public class HotKeyData
    {
        public HotKeyData(ModifierKeys modifiers, Key key)
        {
            Modifiers = modifiers;
            Key = key;
        }

        public HotKeyData() : this(ModifierKeys.None, Key.None) {}

        public ModifierKeys Modifiers { get; set; }
        public Key Key { get; set; }
        public GamePadButton GamePadButton { get; set; }

        public bool IsValid => Key != Key.None || GamePadButton != GamePadButton.None;
        public string KeyString => KeyUtils.KeyToString(Modifiers, Key);

        public static uint GetCode(ModifierKeys modifiers, Key key)
        {
            return ((uint)modifiers << 16) | (uint)key;
        }

        public uint Code
        {
            get
            {
                if (GamePadButton != GamePadButton.None)
                {
                    return 0xF0000000 | (uint)GamePadButton;
                }
                else
                {
                    return GetCode(Modifiers, Key);
                }
            }
        }

        public void SetFromCode(uint code)
        {
            if ((code >> 24) == 0xF0)
            {
                // keep bits 16-23, LeftTrigger and RightTrigger live there
                GamePadButton = (GamePadButton)(code & 0xFFFFFF);
                Modifiers = ModifierKeys.None;
                Key = Key.None;
            }
            else
            {
                GamePadButton = GamePadButton.None;
                Modifiers = (ModifierKeys)(code >> 16);
                Key = (Key)(code & 0xFF);
            }
        }

        public void SetKey(ModifierKeys modifiers, Key key)
        {
            Modifiers = modifiers;
            Key = key;
        }

        public override string ToString()
        {
            return KeyString;
        }
    }


    public class HotKeyItem : HotKeyData
    {
        public string Label { get; set; }
        public Action Action { get; set; }
        /// <summary>
        /// 0: no repeat while held, greater than 0: repeat interval in milliseconds while held
        /// </summary>
        public int RepeatMs { get; set; }
        /// <summary>
        /// time of the last trigger, in milliseconds
        /// </summary>
        public long LastTriggerMs { get; set; }
        /// <summary>
        /// the key is held down
        /// </summary>
        public bool IsPressed { get; set; }
        public bool RunOnGameThread { get; set; } = true;

        public HotKeyItem WithKey(ModifierKeys modifiers, Key key)
        {
            return new(Label, modifiers, key, Action);
        }

        public HotKeyItem(ModifierKeys modifiers, Key key, Action action) : this("", modifiers, key, action)
        {
        }

        public HotKeyItem(string label, ModifierKeys modifiers, Key key, Action action) : base(modifiers, key)
        {
            Label = label;
            Action = action;
        }

        public HotKeyItem() : this("", ModifierKeys.None, Key.None, EmptyAction)
        {
        }

        private static void EmptyAction()
        {
        }
    }
}
