using System;
using System.IO;
using System.Net;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using System.Diagnostics;
using System.Threading;
using static Microsoft.VisualBasic.Interaction;

namespace PebbleInjector
{
    public partial class MainWindow : Window
    {
        private string _status = "";
        private bool _done = true;
        private int _ticks = 0;
        private ConnectionState _connectionState = ConnectionState.None;
        private bool _consoleVisible = false;
        private ConsoleWindow console = new ConsoleWindow();

        // Default DLL download URL (can be overridden)
        private const string DEFAULT_DLL_URL = "https://horion.download/bin/Horion.dll";

        public MainWindow()
        {
            InitializeComponent();
            
            // Set version label
            VersionLabel.Content = $"v{Assembly.GetExecutingAssembly().GetName().Version.Major}.{Assembly.GetExecutingAssembly().GetName().Version.Minor}";
            
            UpdateConnectionState();

            // Check connection on startup
            Task.Run(() =>
            {
                if (CheckConnection())
                {
                    SetStatus("checking for updates...");
                }
            });
        }

        private void UpdateConnectionState()
        {
            switch (_connectionState)
            {
                case ConnectionState.None:
                    ConnectionStateLabel.Content = "Not connected";
                    ConnectionStateLabel.Foreground = System.Windows.Media.Brushes.White;
                    break;
                case ConnectionState.Connected:
                    ConnectionStateLabel.Content = "Connected";
                    ConnectionStateLabel.Foreground = System.Windows.Media.Brushes.ForestGreen;
                    break;
                case ConnectionState.Disconnected:
                    ConnectionStateLabel.Content = "Disconnected";
                    ConnectionStateLabel.Foreground = System.Windows.Media.Brushes.Coral;
                    break;
            }
        }

        private void SetStatus(string status)
        {
            _done = false;
            _status = status;
            
            // Update UI on main thread
            Dispatcher.Invoke(() => InjectButton.Content = $"{_status}...");
            
            Console.WriteLine($"[Status] {status}");
        }

        private void Done()
        {
            _done = true;
            _status = "";
            _ticks = 0;
            Dispatcher.Invoke(() => InjectButton.Content = "inject");
            SetStatus("done");
        }

        // Left click: Download and inject default DLL
        private async void InjectButton_Left(object sender, MouseButtonEventArgs e)
        {
            if (!_done) return;

            SetStatus("checking connection...");
            
            if (!CheckConnection())
            {
                var result = MessageBox.Show(
                    "Can't reach download server. Try anyways?", 
                    null, 
                    MessageBoxButton.YesNo);
                
                if (result == MessageBoxResult.No)
                {
                    Done();
                    return;
                }
            }

            SetStatus("downloading DLL...");
            
            var tempPath = Path.Combine(Path.GetTempPath(), "PebbleInjector.dll");
            
            try
            {
                using (var wc = new WebClient())
                {
                    await wc.DownloadFileTaskAsync(new Uri(DEFAULT_DLL_URL), tempPath);
                }

                // Auto-detect Minecraft process and inject
                var processes = Process.GetProcessesByName("Minecraft.Windows");
                
                if (processes.Length == 0)
                {
                    MessageBox.Show("Minecraft not running. Launching...");
                    
                    Task.Run(() =>
                    {
                        try
                        {
                            Shell("explorer.exe shell:appsFolder\\Microsoft.MinecraftUWP_8wekyb3d8bbwe!App", Wait: false);
                            
                            // Wait for process to start
                            int waitCount = 0;
                            while (processes.Length == 0 && waitCount < 200)
                            {
                                processes = Process.GetProcessesByName("Minecraft.Windows");
                                Thread.Sleep(10);
                                waitCount++;
                            }

                            if (processes.Length > 0)
                            {
                                SetStatus($"injecting into PID: {processes[0].Id}");
                                InjectorBase.Inject(tempPath, (uint)processes[0].Id);
                            }
                        }
                        catch (Exception ex)
                        {
                            MessageBox.Show($"Launch failed: {ex.Message}");
                        }
                    }).Wait();
                }
                else
                {
                    SetStatus($"injecting into PID: {processes[0].Id}");
                    InjectorBase.Inject(tempPath, (uint)processes[0].Id);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Download failed: {ex.Message}");
            }

            Done();
        }

        // Right click: Use custom DLL path
        private void InjectButton_Right(object sender, MouseButtonEventArgs e)
        {
            if (!_done) return;

            var dialog = new OpenFileDialog
            {
                Filter = "DLL files (*.dll)|*.dll",
                RestoreDirectory = true
            };

            if (dialog.ShowDialog().GetValueOrDefault())
            {
                SetStatus("selecting DLL...");
                
                // Auto-detect Minecraft process and inject
                var processes = Process.GetProcessesByName("Minecraft.Windows");
                
                if (processes.Length == 0)
                {
                    MessageBox.Show("Minecraft not running. Launch it first, then click again.");
                    Done();
                    return;
                }

                SetStatus($"injecting into PID: {processes[0].Id}");
                InjectorBase.Inject(dialog.FileName, (uint)processes[0].Id);
            }
            else
            {
                Done();
            }
        }

        private bool CheckConnection()
        {
            try
            {
                var request = (HttpWebRequest)WebRequest.Create("https://horion.download");
                request.KeepAlive = false;
                request.Timeout = 1000;
                using (request.GetResponse()) 
                    return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        private void CloseWindow(object sender, MouseButtonEventArgs e) => Application.Current.Shutdown();
        
        private void DragWindow(object sender, MouseButtonEventArgs e) => DragMove();

        private void ConsoleButton_Click(object sender, RoutedEventArgs e)
        {
            // Toggle console visibility (currently hidden - placeholder for future implementation)
            if (_consoleVisible == false)
                _consoleVisible = true;
            else
                _consoleVisible = false;
        }
    }

    // Connection state enum
    public enum ConnectionState { None, Connected, Disconnected }
}
