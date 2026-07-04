#include "hooks/D3D12RenderHook.h"

#include <windows.h>

namespace pebble
{
    D3D12RenderHook::D3D12RenderHook(GameBuild build)
        : build_(build)
    {
    }

    bool D3D12RenderHook::Start()
    {
        const bool hasDxgi = GetModuleHandleW(L"dxgi.dll") != nullptr;
        const bool hasD3D12 = GetModuleHandleW(L"d3d12.dll") != nullptr;
        if (!hasDxgi || !hasD3D12)
        {
            status_ = HookStatus::BackendUnavailable;
            OutputDebugStringA("PebbleCore: D3D12 render backend is not loaded\n");
            return true;
        }

        // This is deliberately a probe-only first milestone. The next step is
        // to resolve and validate IDXGISwapChain::Present for this exact build
        // before installing a detour.
        status_ = HookStatus::ScaffoldReady;
        const std::string message =
            "PebbleCore: D3D12 cosmetics hook scaffold ready for Minecraft " +
            build_.ToString() + "\n";
        OutputDebugStringA(message.c_str());
        return true;
    }

    void D3D12RenderHook::Stop()
    {
        status_ = HookStatus::NotStarted;
    }

    HookStatus D3D12RenderHook::Status() const
    {
        return status_;
    }
}
