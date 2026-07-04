using System.Windows;

namespace PebbleInjector
{
    public partial class App : Application
    {
        // Register ConsoleWindow type for XAML lookup
        static App()
        {
            var consoleType = typeof(ConsoleWindow);
        }
    }
}
