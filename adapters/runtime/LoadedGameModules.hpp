#pragma once
#include <hksr/native/LoadedModule.hpp>
#include <sstream>

namespace hksr::api::detail {
inline native::LoadedModule GameModule() {
    return native::ReadModule(L"GameAssembly.dll");
}
inline native::LoadedModule UnityModule() {
    return native::ReadModule(L"UnityPlayer.dll");
}
// Image metadata is observable context, never an interface qualification.
inline std::string ImageDiagnostic(const native::LoadedModule& game, const native::LoadedModule& unity) {
    std::ostringstream out;
    out << "observed images: GameAssembly=" << std::hex << game.timestamp << ":" << game.size
        << " UnityPlayer=" << unity.timestamp << ":" << unity.size;
    return out.str();
}
}
