#pragma once

#include "core/GameBuild.h"
#include "hooks/IHook.h"

namespace pebble
{
    class D3D12RenderHook final : public IHook
    {
    public:
        explicit D3D12RenderHook(GameBuild build);

        bool Start() override;
        void Stop() override;
        HookStatus Status() const override;

    private:
        GameBuild build_;
        HookStatus status_ = HookStatus::NotStarted;
    };
}
