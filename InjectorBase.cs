using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;

namespace PebbleInjector
{
    public static class InjectorBase
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, uint processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr VirtualAllocEx(
            IntPtr process,
            IntPtr address,
            UIntPtr size,
            uint allocationType,
            uint protection);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WriteProcessMemory(
            IntPtr process,
            IntPtr baseAddress,
            byte[] buffer,
            UIntPtr size,
            out UIntPtr bytesWritten);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
        private static extern IntPtr GetProcAddress(IntPtr module, string procedureName);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr GetModuleHandle(string moduleName);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr CreateRemoteThread(
            IntPtr process,
            IntPtr threadAttributes,
            UIntPtr stackSize,
            IntPtr startAddress,
            IntPtr parameter,
            uint creationFlags,
            out uint threadId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool VirtualFreeEx(
            IntPtr process,
            IntPtr address,
            UIntPtr size,
            uint freeType);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetExitCodeThread(IntPtr thread, out uint exitCode);

        private const uint ProcessCreateThread = 0x0002;
        private const uint ProcessVmOperation = 0x0008;
        private const uint ProcessVmRead = 0x0010;
        private const uint ProcessVmWrite = 0x0020;
        private const uint ProcessQueryInformation = 0x0400;
        private const uint MemCommit = 0x1000;
        private const uint MemReserve = 0x2000;
        private const uint MemRelease = 0x8000;
        private const uint PageReadWrite = 0x04;
        private const uint WaitObject0 = 0;
        private const uint WaitTimeout = 0x102;

        public static void Inject(string dllPath, uint processId, Action<string> reportStatus = null)
        {
            if (!File.Exists(dllPath))
            {
                throw new FileNotFoundException("The selected DLL was not found.", dllPath);
            }

            dllPath = Path.GetFullPath(dllPath);
            Report(reportStatus, "setting file permissions");
            GrantAppContainerReadAccess(dllPath);

            Report(reportStatus, $"finding process (PID: {processId})");
            Process process;
            try
            {
                process = Process.GetProcessById(checked((int)processId));
                if (process.HasExited)
                {
                    throw new InvalidOperationException("The target process has already exited.");
                }
            }
            catch (ArgumentException ex)
            {
                throw new InvalidOperationException($"No running process has PID {processId}.", ex);
            }

            try
            {
                if (IsModuleLoaded(process, dllPath))
                {
                    throw new InvalidOperationException("That DLL is already loaded in Minecraft.");
                }

                InjectCore(process, dllPath, reportStatus);
            }
            finally
            {
                process.Dispose();
            }
        }

        private static void InjectCore(Process process, string dllPath, Action<string> reportStatus)
        {
            Report(reportStatus, $"injecting into PID: {process.Id}");
            var access = ProcessCreateThread | ProcessVmOperation | ProcessVmRead |
                         ProcessVmWrite | ProcessQueryInformation;
            var processHandle = OpenProcess(access, false, checked((uint)process.Id));
            if (processHandle == IntPtr.Zero)
            {
                ThrowLastWin32Error("Could not open the Minecraft process");
            }

            IntPtr remotePath = IntPtr.Zero;
            IntPtr threadHandle = IntPtr.Zero;
            var threadCompleted = false;
            try
            {
                var pathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
                var pathSize = new UIntPtr((uint)pathBytes.Length);
                remotePath = VirtualAllocEx(
                    processHandle,
                    IntPtr.Zero,
                    pathSize,
                    MemCommit | MemReserve,
                    PageReadWrite);
                if (remotePath == IntPtr.Zero)
                {
                    ThrowLastWin32Error("Could not allocate memory in the Minecraft process");
                }

                UIntPtr bytesWritten;
                if (!WriteProcessMemory(processHandle, remotePath, pathBytes, pathSize, out bytesWritten) ||
                    bytesWritten.ToUInt64() != (ulong)pathBytes.Length)
                {
                    ThrowLastWin32Error("Could not write the DLL path to the Minecraft process");
                }

                var kernel32 = GetModuleHandle("kernel32.dll");
                var loadLibrary = kernel32 == IntPtr.Zero
                    ? IntPtr.Zero
                    : GetProcAddress(kernel32, "LoadLibraryW");
                if (loadLibrary == IntPtr.Zero)
                {
                    ThrowLastWin32Error("Could not locate LoadLibraryW");
                }

                uint threadId;
                threadHandle = CreateRemoteThread(
                    processHandle,
                    IntPtr.Zero,
                    UIntPtr.Zero,
                    loadLibrary,
                    remotePath,
                    0,
                    out threadId);
                if (threadHandle == IntPtr.Zero)
                {
                    ThrowLastWin32Error("Could not start the injection thread");
                }

                var waitResult = WaitForSingleObject(threadHandle, 15000);
                threadCompleted = waitResult == WaitObject0;
                if (waitResult == WaitTimeout)
                {
                    throw new TimeoutException("Minecraft did not finish loading the DLL within 15 seconds.");
                }
                if (!threadCompleted)
                {
                    ThrowLastWin32Error("Waiting for the injection thread failed");
                }

                uint exitCode;
                if (!GetExitCodeThread(threadHandle, out exitCode) || exitCode == 0)
                {
                    throw new InvalidOperationException(
                        "Minecraft rejected the DLL. Confirm that the DLL is valid and uses the x64 architecture.");
                }

                Report(reportStatus, "done");
            }
            finally
            {
                if (threadHandle != IntPtr.Zero)
                {
                    CloseHandle(threadHandle);
                }

                // A timed-out thread may still be reading this buffer.
                if (remotePath != IntPtr.Zero && threadCompleted)
                {
                    VirtualFreeEx(processHandle, remotePath, UIntPtr.Zero, MemRelease);
                }

                CloseHandle(processHandle);
            }
        }

        private static void GrantAppContainerReadAccess(string dllPath)
        {
            try
            {
                var fileInfo = new FileInfo(dllPath);
                var accessControl = fileInfo.GetAccessControl();
                accessControl.AddAccessRule(new FileSystemAccessRule(
                    new SecurityIdentifier("S-1-15-2-1"),
                    FileSystemRights.ReadAndExecute,
                    InheritanceFlags.None,
                    PropagationFlags.None,
                    AccessControlType.Allow));
                fileInfo.SetAccessControl(accessControl);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException(
                    "Could not grant Minecraft permission to read the DLL. Try running the injector as administrator.",
                    ex);
            }
        }

        private static bool IsModuleLoaded(Process process, string dllPath)
        {
            try
            {
                var expectedPath = Path.GetFullPath(dllPath);
                foreach (ProcessModule module in process.Modules)
                {
                    if (string.Equals(module.FileName, expectedPath, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }
            catch
            {
                // Packaged processes can deny module enumeration; injection may still work.
            }

            return false;
        }

        private static void ThrowLastWin32Error(string message)
        {
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), message);
        }

        private static void Report(Action<string> reportStatus, string status)
        {
            reportStatus?.Invoke(status);
        }
    }
}
