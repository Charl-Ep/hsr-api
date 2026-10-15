#pragma once
#include <hksr/native/LoadedModule.hpp>
#include <hksr/runtime/GameVersion.hpp>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
// Image identity is a scope, not a statement that every engine call has passed.
// Shared algorithms remain shared; never duplicate an adapter merely for RVAs.
struct RuntimeProfile {
    RuntimeVersion version;
    DWORD game_timestamp,game_size,unity_timestamp,unity_size;
    bool legacy_export;
};
inline constexpr RuntimeProfile Hsr450{RuntimeVersion::Hsr450,0x6A7CCA0A,0x208AC000,0x6A7063D9,0x02741000,true};
inline constexpr RuntimeProfile Hsr460{RuntimeVersion::Hsr460,0x6AAAE554,0x20FD9000,0x6A9E833C,0x02765000,false};
inline const RuntimeProfile& RuntimeProfileFor(RuntimeVersion version) {
    switch(version){case RuntimeVersion::Hsr450:return Hsr450;case RuntimeVersion::Hsr460:return Hsr460;}
    throw std::invalid_argument("unknown game runtime profile");
}
inline native::LoadedModule GameModule(const RuntimeProfile& profile) {
    return native::ReadModule(L"GameAssembly.dll",profile.game_timestamp,profile.game_size);
}
inline native::LoadedModule UnityModule(const RuntimeProfile& profile) {
    return native::ReadModule(L"UnityPlayer.dll",profile.unity_timestamp,profile.unity_size);
}
inline const RuntimeProfile& CurrentRuntimeProfile() {
    // Automatic production entry follows this SDK release's target series.
    // Historical profiles require explicit selection for low-level research.
    GameModule(Hsr460);UnityModule(Hsr460);return Hsr460;
}
}
