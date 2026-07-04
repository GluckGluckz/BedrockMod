# PebbleInjector

An x64 WPF launcher for loading a compatible utility DLL into Minecraft:
Bedrock Edition on Windows.

## Build

Requirements:

- Windows
- .NET Framework 4.8 Developer Pack
- A recent .NET SDK or Visual Studio with the .NET desktop workload

```powershell
dotnet build PebbleInjector.csproj -c Release -p:Platform=x64
```

The executable is written to `bin\x64\Release\net48\PebbleInjector.exe`.

## Use

- Click **inject** to choose a local DLL. Minecraft is launched automatically if
  it is not already running.
- Click the terminal icon to view operation details.

Only load DLLs you trust and have permission to use. The injector and target DLL
must both use the x64 architecture.
