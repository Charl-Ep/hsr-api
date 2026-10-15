#include <hksr/model/ModelRelease.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace hksr::api;
int component, other_component, metadata;
auto* model = reinterpret_cast<CharacterModelComponent*>(&component);
unsigned originals = 0, failures = 0;
bool nested = false, original_fault = false, consumer_fault = false;
std::vector<int> order;
void Check(bool ok) { if (!ok) throw std::runtime_error("model release test failed"); }
__declspec(noinline) void __fastcall Root(CharacterModelComponent*, void* info) {
    Check(info == &metadata); ++originals; order.push_back(4);
    if (original_fault) RaiseException(0xE0480001, 0, 0, nullptr);
}
__declspec(noinline) void __fastcall Art(CharacterModelComponent* value, bool option, void* info) {
    Check(option && info == &metadata); ++originals; order.push_back(2);
    if (nested) Root(value, info);
}
void Before(const ModelReleaseEvent& event, void* context) {
    Check(event.binding.model == model && event.binding.generation && context == &component);
    if (event.boundary == ModelReleaseBoundary::ArtUnload) { Check(event.art_unload_option); order.push_back(1); }
    else order.push_back(3);
    if (consumer_fault) throw std::runtime_error("restore failed");
}
void Failure(const ModelReleaseEvent&, const char*, void*) noexcept { ++failures; }
bool InvokeRoot() {
    __try { Root(model, &metadata); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct Resolver : MethodResolver {
    Method Resolve(const char* space, const char* owner, const char* name, const char* result, std::initializer_list<const char*> parameters, MethodKind kind) override {
        Check(std::string(space) == "RPG.GameCore" && std::string(owner) == "CharacterModelComponent");
        Check(std::string(result) == "System.Void" && kind == MethodKind::Instance);
        if (std::string(name) == "OnArtNodeUnload") { Check(parameters.size() == 1 && std::string(*parameters.begin()) == "System.Boolean"); return {reinterpret_cast<void*>(&Art), &metadata}; }
        Check(std::string(name) == "_ReleaseRootModel" && parameters.size() == 0);
        return {reinterpret_cast<void*>(&Root), &metadata};
    }
};
}
void TestModelRelease() {
    Resolver resolver; InstallModelReleaseHooks(resolver);
    auto token = RegisterModelRelease(model, {Before, Failure, &component});
    nested = true; Art(model, true, &metadata);
    Check(order == std::vector<int>({1,2,3,4}) && originals == 2);
    order.clear(); Root(reinterpret_cast<CharacterModelComponent*>(&other_component), &metadata);
    Check(order == std::vector<int>({4}));
    consumer_fault = true; order.clear(); Check(InvokeRoot() && failures == 1);
    Check(order == std::vector<int>({3,4})); consumer_fault = false;
    original_fault = true; Check(!InvokeRoot() && failures == 1); original_fault = false;
    ForgetModelRelease(token); const auto replacement = RegisterModelRelease(model, {Before, Failure, &component});
    Check(token.generation != replacement.generation);
    ForgetModelRelease(token); order.clear(); Check(InvokeRoot()); Check(order == std::vector<int>({3,4}));
    ForgetModelRelease(replacement); order.clear(); Check(InvokeRoot()); Check(order == std::vector<int>({4}));
    RemoveModelReleaseHooksAtQuiescentPoint();
    // Other hook services own their own registrations and are not removed.
    InstallModelReleaseHooks(resolver); RemoveModelReleaseHooksAtQuiescentPoint();
    std::cout << "Model release hook tests passed: pre-release order, nested boundaries, generations and original errors.\n";
}
