using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;

namespace PebbleInjector.Images
{
    class Program
    {
        static void Main()
        {
            // Create console.png (simple terminal icon)
            using (var img = new Bitmap(20, 20))
            {
                using (var g = Graphics.FromImage(img))
                {
                    g.Clear(Color.Black);
                    
                    // Draw terminal window outline
                    Pen pen = new Pen(Color.White, 1.5f);
                    g.DrawRectangle(pen, 1, 1, 18, 18);
                    
                    // Draw three horizontal lines (terminal text)
                    using (Brush brush = new SolidBrush(Color.FromArgb(200, Color.Gray)))
                    {
                        g.FillRectangle(brush, 4, 6, 12, 2);
                        g.FillRectangle(brush, 4, 9, 12, 2);
                        g.FillRectangle(brush, 4, 12, 12, 2);
                    }
                }
                
                img.Save("C:\\dev\\PebbleInjector\\Injector-master\\PebbleInjector\\console.png", ImageFormat.Png);
            }

            // Create banner image (solid green background)
            using (var banner = new Bitmap(300, 60))
            {
                using (var g = Graphics.FromImage(banner))
                {
                    // Solid green gradient-like effect - use System.Drawing namespace explicitly
                    var brush = new LinearGradientBrush(
                        new Point(0, 0), 
                        new Point(300, 0), 
                        Color.FromArgb(100, 72, 154, 94), // Green start
                        Color.FromArgb(180, 72, 154, 94)); // Green end
                    
                    g.FillRectangle(brush, new Rectangle(0, 0, 300, 60));
                    
                    // Add text "PebbleInjector"
                    using (var font = new Font("Bahnschrift", 28, FontStyle.Bold))
                    {
                        using (Brush b = Brushes.White)
                        {
                            g.DrawString("PebbleInjector", font, b, 40, 15);
                        }
                    }
                }
                
                banner.Save("pebble-banner.png", ImageFormat.Png);
            }

            Console.WriteLine("Images created successfully!");
        }
    }
}
