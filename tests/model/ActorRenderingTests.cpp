#include <hksr/model/ActorRendering.hpp>
#include "../../adapters/model/RenderingCalls.hpp"
#include "../../src/model/ActorRenderingNative.hpp"
#include <iostream>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool value) {
    if (!value)
        throw std::runtime_error("rendering contract");
}
struct Receiver {
    std::uint32_t mask = 0;
    bool visible = true, reset = false;
    detail::RenderingVisibilityInput input;
    void* add = nullptr;
    void* remove = nullptr;
};
void __fastcall Mask(Receiver* receiver, std::uint32_t value, void*) {
    receiver->mask = value;
}
void __fastcall Visible(Receiver* receiver, bool value, detail::RenderingVisibilityInput* input, void*) {
    receiver->visible = value;
    receiver->input = *input;
}
void __fastcall Outline(Receiver* receiver, bool reset, void* add, void* remove, void*) {
    receiver->reset = reset;
    receiver->add = add;
    receiver->remove = remove;
}
template <class T> Method M(T call) {
    return {reinterpret_cast<void*>(call), nullptr};
}
struct BindingResolver : MethodResolver {
    std::string missing;
    Method Resolve(const char*, const char*, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        if (name == missing)
            throw std::runtime_error("synthetic declaration unavailable");
        return M(Mask);
    }
};
void TestPartialBindings() {
    BindingResolver resolver;
    std::string missing_type;
    const auto type = [&](const char*, const char* name) -> void* {
        if (name == missing_type)
            throw std::runtime_error("synthetic type unavailable");
        return &resolver;
    };
    const auto fields = [](const char* ns, const char* owner, bool) -> std::vector<DeclaredField> {
        const std::string name = owner;
        if (name == "ANACIPLIMDE")
            return {{"", name, "KDOLHICFKLL", "System.String", 16, 6, true},
                    {"", name, "LPEFHLMHLPN", "System.Boolean", 24, 6, true},
                    {"", name, "ONMFCPIJGNP", "System.Boolean", 25, 6, true},
                    {"", name, "IEMGPGLKIIG", "System.Boolean", 26, 6, true}};
        if (name == "CharacterEffect")
            return {{ns, name, "NIOHCEKBNFF", "System.Collections.Generic.List<UnityEngine.Renderer>", 16, 6, true},
                    {ns, name, "FNEIHJDCMMM", "System.Collections.Generic.List<UnityEngine.Renderer>", 24, 6, true}};
        return {{"RPG.Client", "BaseShaderPropertyTransition", "OIDAGBPPNBJ", "DEBEAKKOHLN", 64, 4, true}};
    };
    const auto outline = [](detail::RenderingBindings&) {
        throw std::runtime_error("synthetic outline list unavailable");
    };
    auto binding = detail::ResolveRenderingBindings(resolver, fields, type, outline);
    Check(!binding.outline_error.empty() && binding.visibility_error.empty() && binding.mask_read_error.empty());
    resolver.missing = "set_renderingLayerMask";
    binding = detail::ResolveRenderingBindings(resolver, fields, type, outline);
    Check(!binding.mask_write_error.empty() && binding.mask_read_error.empty() && binding.visibility_error.empty());
    resolver.missing = "";
    missing_type = "Renderer";
    binding = detail::ResolveRenderingBindings(resolver, fields, type, outline);
    Check(!binding.renderer_error.empty() && binding.visibility_error.empty());
    resolver.missing = "op_Implicit";
    missing_type = "";
    binding = detail::ResolveRenderingBindings(resolver, fields, type, outline);
    Check(!binding.renderer_error.empty() && binding.visibility_error.empty());
    resolver.missing = "";
    missing_type = "CharacterModelComponent";
    binding = detail::ResolveRenderingBindings(resolver, fields, type, outline);
    Check(!binding.model_error.empty() && binding.visibility_error.empty());
    resolver.missing = "SetExtraTargetRenders";
    missing_type = "";
    binding = detail::ResolveRenderingBindings(resolver, fields, type, [](detail::RenderingBindings&) {});
    Check(binding.outline_error.empty() && !binding.outline_write_error.empty() && binding.visibility_error.empty());
}

}
int main() try {
    TestPartialBindings();
    Receiver state;
    int key = 0, add = 0, remove = 0;
    unsigned queries = 0;
    const auto read = [&](const char* ns, const char* owner, bool parents) {
        ++queries;
        Check(std::string(ns) == "RPG.Client" && std::string(owner) == "BaseShaderPropertyTransition" && !parents);
        return std::vector<DeclaredField>{
            {"RPG.Client", "BaseShaderPropertyTransition", "OIDAGBPPNBJ", "DEBEAKKOHLN", 64, 4, true}};
    };
    Check(detail::ResolveRenderingRendererCacheOffset(read) == 64 && queries == 1);
    bool wrong_owner = false;
    try {
        detail::ResolveRenderingRendererCacheOffset([](const char*, const char*, bool) {
            return std::vector<DeclaredField>{
                {"RPG.Client", "CharacterShaderPropertyTransition", "OIDAGBPPNBJ", "DEBEAKKOHLN", 64, 4, true}};
        });
    } catch (const std::runtime_error&) {
        wrong_owner = true;
    }
    Check(wrong_owner);
    Check(detail::SetRenderingMask(M(Mask), &state, 525 | 64).returned && state.mask == (525 | 64));
    Check(detail::SetRenderingVisibility(M(Visible), &state, &key, false).returned && !state.visible &&
          state.input.key == &key && !state.input.collider && !state.input.trigger && !state.input.audio);
    Check(detail::SetRenderingVisibility(M(Visible), &state, &key, true).returned && state.visible &&
          state.input.key == &key);
    Check(detail::SetRenderingOutline(M(Outline), &state, &add, &remove).returned && state.reset && state.add == &add &&
          state.remove == &remove);
    bool failed = false;
    ActorRenderingServices calls;
    calls.current = [] { return true; };
    calls.visible = [](const auto&, bool) {
        return RenderingWriteResult{RenderingWriteState::Failed, "actual native failure"};
    };
    calls.renderers = [](const auto&) -> std::optional<std::vector<RendererMaskValue>> {
        throw std::runtime_error("native observation failed");
    };
    ActorRendering api(std::move(calls));
    Check(api.Current() && api.Visible("own", false).state == RenderingWriteState::Failed);
    try {
        api.Renderers();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Check(failed);
    failed = false;
    try {
        api.Visible("", false);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    Check(failed);
    std::cout << "Rendering native ABI and typed forwarding passed; no game qualification\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
