#pragma once
#include "../Export.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace hksr::api {
struct RenderingEpoch {
    std::uintptr_t model = 0, art = 0, root = 0;
    bool operator==(const RenderingEpoch& b) const {
        return model == b.model && art == b.art && root == b.root;
    }
};
struct RendererMaskValue {
    RenderingEpoch epoch;
    std::uintptr_t renderer = 0;
    std::uint32_t value = 0;
};
struct OutlineTargets {
    RenderingEpoch epoch;
    std::uintptr_t effect = 0;
    std::vector<std::uintptr_t> add, remove;
};
enum class RenderingWriteState { Applied, Retired, Failed };
struct RenderingWriteResult {
    RenderingWriteState state = RenderingWriteState::Failed;
    std::string detail;
};
struct ActorRenderingServices {
    std::function<std::optional<std::vector<RendererMaskValue>>(const std::string&)> renderers;
    std::function<RenderingWriteResult(const RendererMaskValue&, std::uint32_t)> mask;
    std::function<std::optional<OutlineTargets>()> outline;
    std::function<std::optional<std::uintptr_t>(const std::string&)> named_renderer;
    std::function<RenderingWriteResult(const OutlineTargets&, const std::vector<std::uintptr_t>&,
                                       const std::vector<std::uintptr_t>&)>
        set_outline;
    std::function<RenderingWriteResult(const std::string&, bool)> visible;
    std::function<bool()> close;
    std::function<bool()> current;
    std::function<std::optional<std::uint32_t>(const RendererMaskValue&)> read_mask;
};
// Immediate official renderer operations. Scope composition belongs to clients.
// Reads throw on native observation failure; null means actual unavailability.
class HKSR_API ActorRendering {
  public:
    explicit ActorRendering(ActorRenderingServices);
    ~ActorRendering(); // no native calls; drain Close on its engine thread
    std::optional<std::vector<RendererMaskValue>> Renderers(const std::string& node = {}) const;
    RenderingWriteResult Mask(const RendererMaskValue&, std::uint32_t);
    std::optional<OutlineTargets> Outline() const;
    std::optional<std::uintptr_t> NamedRenderer(const std::string&) const;
    RenderingWriteResult SetOutline(const OutlineTargets&, const std::vector<std::uintptr_t>& add,
                                    const std::vector<std::uintptr_t>& remove);
    // Pure rendering only: collider/trigger/audio flags are always false.
    // false adds this private key; true removes only the same key.
    RenderingWriteResult Visible(const std::string& private_key, bool);
    bool Close();
    bool Current() const;
    std::optional<std::uint32_t> ReadMask(const RendererMaskValue&) const;

  private:
    ActorRenderingServices calls_;
};
}
