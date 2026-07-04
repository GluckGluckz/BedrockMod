using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Threading;
using System.Windows;

namespace PebbleInjector
{
    // P/Invoke declarations for DLL injection
    public static class InjectorBase
    {
        [DllImport("kernel32.dll")]
        public static extern IntPtr OpenProcess(IntPtr dwDesiredAccess, bool bInheritHandle, uint processId);

        [DllImport("kernel32.dll")]
        public static extern bool CloseHandle(IntPtr hObject);

        [DllImport("kernel32.dll")]
        public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);

        [DllImport("kernel32.dll")]
        public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, char[] lpBuffer, int nSize, out IntPtr lpNumberOfBytesWritten);

        [DllImport("kernel32.dll")]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

        [DllImport("kernel32.dll")]
        public static extern IntPtr GetModuleHandle(string lpModuleName);

        [DllImport("kernel32.dll")]
        public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, ref IntPtr lpThreadId);

        [DllImport("kernel32.dll")]
        public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll")]
        public static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress, int dwSize, IntPtr dwFreeType);

        // Process access flags
        public const int PROCESS_VM_OPERATION = 0x0008;
        public const int PROCESS_VM_WRITE = 0x0020;
        public const int PROCESS_QUERY_INFORMATION = 0x0400;
        public const int PROCESS_ALL_ACCESS = 0xFFFF;

        // Memory protection flags
        public const int PAGE_READWRITE = 0x04;
        public const int PAGE_EXECUTE_READWRITE = 0x40;

        [DllImport("user32.dll")]
        public static extern IntPtr FindWindow(String lpClassName, String lpWindowName);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        // Inject DLL into a running process
        public static void Inject(string dllPath, uint processId)
        {
            if (!File.Exists(dllPath))
            {
                MessageBox.Show($"DLL not found: {dllPath}");
                return;
            }

            SetStatus("setting file perms");
            try
            {
                var fileInfo = new FileInfo(dllPath);
                var accessControl = fileInfo.GetAccessControl();
                accessControl.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier("S-1-15-2-1"), FileSystemRights.FullControl, InheritanceFlags.None, PropagationFlags.NoPropagateInherit, AccessControlType.Allow));
                fileInfo.SetAccessControl(accessControl);
            }
            catch (Exception)
            {
                MessageBox.Show("Could not set permissions, try running the injector as admin.");
                return;
            }

            SetStatus($"finding process (PID: {processId})");
            var processes = Process.GetProcessesByName(ProcessNameFromPath(processId));
            
            if (processes.Length == 0)
            {
                MessageBox.Show($"Process not found with PID: {processId}");
                return;
            }

            var process = processes.First(p => p.Id == processId && p.Responding);

            // Check if already injected
            for (int i = 0; i < process.Modules.Count; i++)
            {
                if (process.Modules[i].FileName != null && process.Modules[i].FileName.Contains(Path.GetFileName(dllPath)))
                {
                    MessageBox.Show("Already injected!");
                    return;
                }
            }

            SetStatus($"injecting into PID: {processId}");
            
            // Open process with injection permissions
            IntPtr handle = OpenProcess((IntPtr)(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION), false, (uint)process.Id);
            
            if (handle == IntPtr.Zero || !process.Responding)
            {
                MessageBox.Show("Failed to get process handle");
                return;
            }

            // Allocate memory for DLL path in target process
            IntPtr p1 = VirtualAllocEx(handle, IntPtr.Zero, (uint)(dllPath.Length + 1), 0x1000U, 0x04U);
            
            if (p1 == IntPtr.Zero)
            {
                MessageBox.Show("Failed to allocate memory");
                return;
            }

            // Write DLL path into allocated memory
            WriteProcessMemory(handle, p1, dllPath.ToCharArray(), dllPath.Length, out IntPtr p2);

            // Get LoadLibraryA address from kernel32.dll in target process
            IntPtr procAddress = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");

            if (procAddress == IntPtr.Zero)
            {
                MessageBox.Show("Failed to get LoadLibraryA address");
                return;
            }

            // Create remote thread to execute LoadLibraryA
            IntPtr p3 = CreateRemoteThread(handle, IntPtr.Zero, 0U, procAddress, p1, 0U, ref p2);

            if (p3 == IntPtr.Zero)
            {
                MessageBox.Show("Failed to create remote thread");
                return;
            }

            // Wait for injection thread to complete
            uint n = WaitForSingleObject(p3, 5000);
            
            // Cleanup allocated memory
            VirtualFreeEx(handle, p1, 0, (IntPtr)0x4000);
            CloseHandle(p3);
            CloseHandle(handle);

            SetStatus("done");
        }

        private static string ProcessNameFromPath(uint pid)
        {
            try
            {
                var process = Process.GetProcessById((int)pid);
                return process.ProcessName;
            }
            catch
            {
                return "Minecraft.Windows";
            }
        }

        public static void SetStatus(string status)
        {
            // Override in derived class
        }
    }
}
