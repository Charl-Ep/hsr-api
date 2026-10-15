#pragma once
#include "../metadata/MethodResolver.hpp"
#include <cstdint>

namespace hksr::sdk_v1 {
struct CharacterModelComponent;
enum class ModelReleaseBoundary { ArtUnload, RootRelease };
struct ModelBindingToken {
    CharacterModelComponent* model = nullptr;
    std::uint64_t generation = 0;
};
struct ModelReleaseEvent {
    ModelBindingToken binding;
    ModelReleaseBoundary boundary;
    bool art_unload_option = false; // raw official argument, no invented meaning
};
struct ModelReleaseSink {
    // Runs synchronously BEFORE the original release. Restore borrowed engine
    // references here; do not queue restoration to a later frame.
    void (*before)(const ModelReleaseEvent&, void*) = nullptr;
    void (*failure)(const ModelReleaseEvent&, const char*, void*) noexcept = nullptr;
    void* context = nullptr;
};
// Registration/forgetting and removal require an engine quiescent point.
// Sink context must outlive in-flight delivery. No ownership of game objects.
HKSR_API ModelBindingToken RegisterModelRelease(CharacterModelComponent* model, ModelReleaseSink sink);
HKSR_API void ForgetModelRelease(ModelBindingToken token);
HKSR_API void InstallModelReleaseHooks(MethodResolver& resolver);
HKSR_API void RemoveModelReleaseHooksAtQuiescentPoint();
}
