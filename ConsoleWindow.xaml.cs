using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace PebbleInjector
{
    public partial class ConsoleWindow : Window
    {
        // Override IsVisible property to check our own Visibility state
        public new bool IsVisible => Visibility == Visibility.Visible;

        public ConsoleWindow()
        {
            InitializeComponent();
            
            // Simple console UI - placeholder for future implementation
            Title = "PebbleInjector Console";
            Width = 400;
            Height = 300;
            WindowStyle = WindowStyle.None;
            AllowsTransparency = true;
            Background = Brushes.Black;
            ResizeMode = ResizeMode.CanResizeWithGrip;

            // Create a simple text area for console output
            var grid = new Grid();
            grid.Children.Add(new TextBlock
            {
                TextWrapping = TextWrapping.Wrap,
                Foreground = Brushes.White,
                Background = Brushes.Black,
                Padding = new Thickness(10),
                FontSize = 12,
                Text = "[Console] Ready for injection..."
            });

            Content = grid;
        }

        // Override Close method to use our Hide() implementation
        public new void Close() => Hide();
    }
}
