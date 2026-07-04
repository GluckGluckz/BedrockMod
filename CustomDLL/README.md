# PebbleCore

PebbleCore is the x64 native DLL loaded by PebbleInjector. The current milestone
is a stable lifecycle and cosmetics shell: it loads outside the Windows loader
lock, detects the Minecraft package build, probes for the D3D12 render backend,
exposes a thread-safe cosmetics registry, and provides a native client menu.

## Build and test

From this directory:

```powershell
cmake --preset x64
cmake --build --preset release
ctest --preset release
```

The injector-ready DLL is written to:

`build\Release\PebbleCore.dll`

## Exported API

- `IsModActive`
- `GetModName`
- `GetModApiVersion`
- `RequestShutdown`
- `GetDetectedGameVersion`
- `GetRenderHookStatus`
- `GetCosmeticCount`
- `SetCosmeticEnabled`
- `IsCosmeticEnabled`
- `IsMenuVisible`
- `ToggleMenu`
- `IsZoomEnabled` / `SetZoomEnabled`
- `IsZoomActive`
- `IsZoomKeybindEnabled` / `SetZoomKeybindEnabled`
- `IsNightVisionAvailable`
- `IsNightVisionEnabled` / `SetNightVisionEnabled`
- `IsNightVisionApplied`

## Adding features

The running development client was detected as package build `1.26.3202.0` with
DXGI/D3D12. `D3D12RenderHook` currently stops at `ScaffoldReady`; it deliberately
does not patch `Present` until that target has been resolved and validated.

Press **Right Shift** to open or close the PebbleCore cosmetics menu. Escape and
the menu's close button also close it. Cosmetic cards are clickable and update
the in-memory cosmetics registry immediately.

The **Render** category contains **Zoom**. Enable the module and press `C` to
toggle 2× zoom. Its `C` keybind can be disabled independently; with the keybind
disabled, enabling the module activates zoom directly.

Zoom input is handled by Minecraft's own window procedure, not by a global
keyboard poll. The zoom surface is layered, non-activating, and mouse
transparent; it automatically closes whenever Minecraft loses foreground focus.

**Night Vision** is a client-memory potion effect, not a desktop color filter.
For package build `1.26.3202.0`, PebbleCore hooks Minecraft's validated
`LocalPlayerChangedConnector` callback and maintains effect ID `16` in the
player's real `MobEffectsComponent`. The module fails closed if that callback is
missing or ambiguous, and restores any pre-existing Night Vision values when
disabled. If PebbleCore is injected after a world is already open, leave and
rejoin that world once so Minecraft publishes the current local-player pointer.

The memory integration uses EnTT at the exact ABI revision used by the researched
1.26.x client layout and MinHook for the validated local-player callback detour.

Cosmetics are registered in `InitializeModules()` and released in reverse order
from `CleanupModules()`. Keep `DllMain` minimal and do not install hooks or wait
on threads from it.
