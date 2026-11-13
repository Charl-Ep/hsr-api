#pragma once
#include <hksr/presentation/audio/VoiceCue.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <functional>
#include <optional>
#include <string>
namespace hksr::api::detail {
struct VoiceResourceAccess {
    std::function<std::string(std::uint32_t, VoiceCue)> read;
    std::function<bool(const std::string&)> exists;
    std::function<std::optional<VoiceCue>(std::int32_t)> ready;
    std::function<bool()> close;
};
VoiceResourceAccess ResolveVoiceResources(MethodResolver&, ObjectMetadata&, ManagedObjects&, ReferenceMethods,
                                          ManagedArrays, ComponentConfig);
}
