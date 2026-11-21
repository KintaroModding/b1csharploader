using System;
using System.IO;
using System.Reflection;
using CSharpManager;
using CSharpModBase;
using CSharpModBase.Input;
using Xunit;

namespace CSharpManager.Tests
{
    public class CSharpModManagerTests
    {
        private static readonly Assembly ModBase = typeof(HotKeyItem).Assembly;

        [Fact]
        public void UsesLoadedAssemblyForSameOrOlderVersion()
        {
            var name = new AssemblyName("csharpmodbase") { Version = new Version(0, 0, 1, 0) };
            Assert.Same(ModBase, CSharpModManager.FindLoadedAssembly(name));
            Assert.Same(ModBase, CSharpModManager.FindLoadedAssembly(new AssemblyName("CSharpModBase")));
        }

        [Fact]
        public void DoesNotUseLoadedAssemblyForNewerVersion()
        {
            var name = new AssemblyName("CSharpModBase") { Version = new Version(99, 0, 0, 0) };
            Assert.Null(CSharpModManager.FindLoadedAssembly(name));
        }

        [Fact]
        public void LoaderPathIsTheFolderOfCSharpModBase()
        {
            Assert.Equal(Path.GetDirectoryName(ModBase.Location), Common.LoaderPath);
            Assert.Equal(Path.Combine(Common.LoaderPath, "Mods"), Common.ModPath);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void DevelopModeLoadsDllAsNewAssembly(bool withSymbols)
        {
            Assembly tests = typeof(CSharpModManagerTests).Assembly;
            string dllPath = tests.Location;
            Assert.True(File.Exists(Path.ChangeExtension(dllPath, ".pdb")), "the build writes a pdb");

            Assembly loaded = CSharpModManager.LoadAsNewAssembly(dllPath, withSymbols);

            Assert.NotSame(tests, loaded);
            Assert.StartsWith("CSharpManager.Tests_", loaded.GetName().Name);
            Assert.NotNull(loaded.GetType(typeof(CSharpModManagerTests).FullName!));
        }
    }
}
