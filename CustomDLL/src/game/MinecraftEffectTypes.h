#pragma once

#include <cstdint>
#include <vector>
#include <entt/entt.hpp>

class EntityId;

struct EntityIdTraits
{
    using value_type = EntityId;
    using entity_type = std::uint32_t;
    using version_type = std::uint16_t;

    static constexpr entity_type entity_mask = 0x3FFFF;
    static constexpr entity_type version_mask = 0x3FFF;
};

namespace entt
{
    template <>
    struct entt_traits<EntityId> : basic_entt_traits<EntityIdTraits>
    {
        static constexpr entity_type page_size = 2048;
    };
}

class EntityId : public entt::entt_traits<EntityId>
{
public:
    entity_type rawId{};

    constexpr EntityId() = default;
    constexpr explicit EntityId(entity_type value) : rawId(value) {}
    constexpr operator entity_type() const { return rawId; }
    constexpr bool operator==(const EntityId& other) const
    {
        return rawId == other.rawId;
    }
};

struct MobEffectInstance
{
    std::int32_t id = 0;
    std::int32_t duration = 0;
    float unknown = 0.0f;
    std::int32_t durationEasy = 0;
    std::int32_t durationNormal = 0;
    std::int32_t durationHard = 0;
    std::uint8_t reserved0[8]{};
    std::int32_t amplifier = 0;
    bool displayOnScreenTextureAnimation = false;
    bool ambient = false;
    bool noCounter = false;
    bool effectVisible = false;
    std::uint8_t reserved1[0x60]{};
};

static_assert(sizeof(MobEffectInstance) == 0x88);

// Declared as `class` (not `struct`) on purpose: Minecraft's RTTI is
// `.?AVMobEffectsComponent@@` (a class), and EnTT derives its component
// type-hash from the MSVC `__FUNCSIG__` type name, which includes the
// `class`/`struct` keyword. Declaring this `struct` produced a different hash
// than Minecraft's, so `registry.try_get<MobEffectsComponent>` never found the
// real pool. Matching the keyword makes the hash line up.
class MobEffectsComponent
{
public:
    std::vector<MobEffectInstance> effects;
};

static_assert(sizeof(MobEffectsComponent) == 0x18);

namespace entt
{
    template <>
    struct component_traits<MobEffectsComponent>
    {
        using type = MobEffectsComponent;
        static constexpr bool in_place_delete = true;
        static constexpr std::size_t page_size = 128;
    };

    template <>
    struct storage_type<MobEffectsComponent, EntityId>
    {
        using type = basic_storage<MobEffectsComponent, EntityId>;
    };
}
