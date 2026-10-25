using b1;
using BtlShare;
using CSharpModBase;
using CSharpModBase.Input;
// using HarmonyLib;

namespace CSharpExample
{
    public class MyMod : ICSharpMod
    {
        public string Name => "ModExample";
        public string Version => "0.0.1";
        // private readonly Harmony harmony;

        public MyMod()
        {
            // harmony = new Harmony(Name);
            // Harmony.DEBUG = true;
        }

        public void Init()
        {
            Log.Info($"{Name} Init");
            Utils.RegisterKeyBind(Key.ENTER, () => Log.Info("Enter pressed"));
            Utils.RegisterKeyBind(ModifierKeys.Control, Key.ENTER, FindPlayer);

            // hook
            // harmony.PatchAll();
        }

        public void DeInit()
        {
            Log.Info($"{Name} DeInit");
            // harmony.UnpatchAll();
        }

        private void FindPlayer()
        {
            Log.Info("Ctrl+Enter pressed");
            var player = MyUtils.GetControlledPawn();
            if (player == null)
            {
                Log.Info("Player not found");
            }
            else
            {
                Log.Info($"Player found: {player}");
                float hp = BGUFunctionLibraryCS.GetAttrValue(player, EBGUAttrFloat.Hp);
                float hpMax = BGUFunctionLibraryCS.GetAttrValue(player, EBGUAttrFloat.HpMax);
                Log.Info($"HP: {hp}/{hpMax}");
            }
        }
    }
}
