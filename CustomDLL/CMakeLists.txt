cmake_minimum_required(VERSION 3.20)

project(PebbleCore VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG 303801c23bf116404fc687bdc103a3a696c17d97
)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
)
FetchContent_MakeAvailable(EnTT minhook)

add_library(PebbleCore SHARED
    src/core/SignatureScanner.cpp
    src/core/SignatureScanner.h
    src/core/GameBuild.cpp
    src/core/GameBuild.h
    src/cosmetics/CosmeticRegistry.cpp
    src/cosmetics/CosmeticRegistry.h
    src/hooks/D3D12RenderHook.cpp
    src/hooks/D3D12RenderHook.h
    src/hooks/IHook.h
    src/game/MinecraftEffectTypes.h
    src/modules/render/NightVisionModule.cpp
    src/modules/render/NightVisionModule.h
    src/modules/render/ZoomModule.cpp
    src/modules/render/ZoomModule.h
    src/modules/combat/AutoArmorModule.cpp
    src/modules/combat/AutoArmorModule.h
    src/modules/combat/TriggerbotModule.cpp
    src/modules/combat/TriggerbotModule.h
    src/modules/player/AutoClickerModule.cpp
    src/modules/player/AutoClickerModule.h
    src/modules/player/AutoBreakModule.cpp
    src/modules/player/AutoBreakModule.h
    src/ui/ClientMenu.cpp
    src/ui/ClientMenu.h
    src/PebbleCore.cpp
    src/PebbleCore.h
)

target_compile_features(PebbleCore PRIVATE cxx_std_20)
target_compile_definitions(PebbleCore PRIVATE
    PEBBLECORE_EXPORTS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_include_directories(PebbleCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(PebbleCore PRIVATE EnTT::EnTT minhook user32 gdi32 Magnification)
set_target_properties(PebbleCore PROPERTIES
    OUTPUT_NAME "PebbleCore"
    DEBUG_POSTFIX ""
)

if(MSVC)
    # /utf-8: source files are UTF-8; without this MSVC decodes them as the
    # system ANSI codepage and mangles literals like L"×" / L"•" / L"✓".
    target_compile_options(PebbleCore PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PebbleCoreLoadTest tests/PebbleCoreLoadTest.cpp)
    add_executable(PebbleCoreSignatureProbe tools/SignatureProbe.cpp)
    target_compile_features(PebbleCoreLoadTest PRIVATE cxx_std_17)
    target_compile_definitions(PebbleCoreLoadTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_features(PebbleCoreSignatureProbe PRIVATE cxx_std_20)
    target_compile_definitions(PebbleCoreSignatureProbe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(PebbleCoreLoadTest PRIVATE /W4 /permissive- /EHsc)
    endif()

    add_test(
        NAME PebbleCore.LoadAndUnload
        COMMAND PebbleCoreLoadTest "$<TARGET_FILE:PebbleCore>"
    )
endif()
