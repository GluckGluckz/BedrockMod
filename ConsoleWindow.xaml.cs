using System;
using System.Windows;
using System.Windows.Input;

namespace PebbleInjector
{
    public partial class ConsoleWindow : Window
    {
        public ConsoleWindow()
        {
            InitializeComponent();
        }

        public void Append(string message)
        {
            ConsoleText.AppendText($"{Environment.NewLine}[{DateTime.Now:HH:mm:ss}] {message}");
            ConsoleText.ScrollToEnd();
        }

        private void DragWindow(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
            {
                DragMove();
            }
        }
    }
}
