namespace CSharpModBase
{
    public static class Common
    {
        public const string LoaderDir = "CSharpLoader";
        public const string ModDir = "CSharpLoader\\Mods";
        public const string DataDir = "CSharpLoader\\Data";
    }


    public interface ICSharpMod
    {
        /// <summary>
        /// mod name
        /// </summary>
        string Name { get; }

        /// <summary>
        /// mod version
        /// </summary>
        string Version { get; }

        /// <summary>
        /// called when the mod is loaded
        /// </summary>
        void Init();
        /// <summary>
        /// called before the manager reloads the mods
        /// </summary>
        void DeInit();
    }
}
