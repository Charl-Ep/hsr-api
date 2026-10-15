#include <hksr/model/ModelRelease.hpp>
#include "../metadata/ResolveBatch.hpp"
#include "../platform/NativeHooks.hpp"
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace hksr::sdk_v1 {
namespace {
struct Registration { ModelBindingToken token; ModelReleaseSink sink; };
std::mutex registrations_mutex;
std::map<CharacterModelComponent*, Registration> registrations;
std::uint64_t sequence = 0;
detail::NativeHooks hooks;

void Before(CharacterModelComponent* model, ModelReleaseBoundary boundary, bool option) {
    Registration registration;
    {
        std::lock_guard<std::mutex> lock(registrations_mutex);
        const auto found = registrations.find(model);
        if (found == registrations.end()) return;
        registration = found->second;
    }
    const ModelReleaseEvent event{registration.token, boundary, option};
    // Never hold registry lock while invoking a consumer. The two official
    // boundaries may nest; consumer restoration must be idempotent.
    try { registration.sink.before(event, registration.sink.context); }
    catch (const std::exception& error) {
        if (registration.sink.failure) registration.sink.failure(event, error.what(), registration.sink.context);
    }
    catch (...) {
        if (registration.sink.failure) registration.sink.failure(event, "model release consumer failed", registration.sink.context);
    }
    // A failed consumer is NOT proof of restoration. Do not cancel the game's
    // disposal or swallow its exception; surface failure through the sink.
}
void __fastcall ArtUnload(CharacterModelComponent* model, bool option, void* metadata) {
    Before(model, ModelReleaseBoundary::ArtUnload, option);
    reinterpret_cast<void(__fastcall*)(CharacterModelComponent*, bool, void*)>(hooks.Original(0))(model, option, metadata);
}
void __fastcall RootRelease(CharacterModelComponent* model, void* metadata) {
    Before(model, ModelReleaseBoundary::RootRelease, false);
    reinterpret_cast<void(__fastcall*)(CharacterModelComponent*, void*)>(hooks.Original(1))(model, metadata);
}
}
ModelBindingToken RegisterModelRelease(CharacterModelComponent* model, ModelReleaseSink sink) {
    if (!model || !sink.before || !sink.failure) throw std::invalid_argument("model release needs a model, consumer and failure reporter");
    std::lock_guard<std::mutex> lock(registrations_mutex);
    if (registrations.count(model)) throw std::logic_error("model already has a release consumer");
    const ModelBindingToken token{model, ++sequence};
    registrations.emplace(model, Registration{token, sink});
    return token;
}
void ForgetModelRelease(ModelBindingToken token) {
    std::lock_guard<std::mutex> lock(registrations_mutex);
    const auto found = registrations.find(token.model);
    if (found != registrations.end() && found->second.token.generation == token.generation) registrations.erase(found);
}
void RemoveModelReleaseHooksAtQuiescentPoint() {
    hooks.Remove();
}
void InstallModelReleaseHooks(MethodResolver& resolver) {
    detail::ResolveBatch batch(resolver);
    const auto art = batch.Resolve("RPG.GameCore", "CharacterModelComponent", "OnArtNodeUnload", "System.Void", {"System.Boolean"});
    const auto root = batch.Resolve("RPG.GameCore", "CharacterModelComponent", "_ReleaseRootModel", "System.Void", {});
    batch.RequireComplete("model release");
    hooks.Install({{"OnArtNodeUnload", art.code, reinterpret_cast<void*>(&ArtUnload)},
        {"_ReleaseRootModel", root.code, reinterpret_cast<void*>(&RootRelease)}});
}
}
