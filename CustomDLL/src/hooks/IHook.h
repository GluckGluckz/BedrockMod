#pragma once

namespace pebble
{
    enum class HookStatus
    {
        NotStarted = 0,
        BackendUnavailable = 1,
        ScaffoldReady = 2,
        Installed = 3,
        Failed = 4
    };

    class IHook
    {
    public:
        virtual ~IHook() = default;
        virtual bool Start() = 0;
        virtual void Stop() = 0;
        virtual HookStatus Status() const = 0;
    };
}
