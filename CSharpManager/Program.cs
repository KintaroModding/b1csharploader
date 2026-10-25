
namespace CSharpManager
{
    internal class Program
    {
        public static void Main(string[] args)
        {
            CSharpModManager manager = new();
            manager.LoadMods();
            manager.ShowStartBanner();
            manager.StartLoop();
        }
    }
}
