#include <hksr/model/ActorRendering.hpp>
#include <stdexcept>
#include <utility>
namespace hksr::api {
ActorRendering::ActorRendering(ActorRenderingServices calls) : calls_(std::move(calls)) {}
ActorRendering::~ActorRendering() = default;
std::optional<std::vector<RendererMaskValue>> ActorRendering::Renderers(const std::string& name) const {
    if (name.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid renderer node");
    return calls_.renderers ? calls_.renderers(name) : std::nullopt;
}
RenderingWriteResult ActorRendering::Mask(const RendererMaskValue& target, std::uint32_t value) {
    return calls_.mask ? calls_.mask(target, value) : RenderingWriteResult{};
}
std::optional<OutlineTargets> ActorRendering::Outline() const {
    return calls_.outline ? calls_.outline() : std::nullopt;
}
std::optional<std::uintptr_t> ActorRendering::NamedRenderer(const std::string& name) const {
    if (name.empty() || name.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid outline renderer name");
    return calls_.named_renderer ? calls_.named_renderer(name) : std::nullopt;
}
RenderingWriteResult ActorRendering::SetOutline(const OutlineTargets& target, const std::vector<std::uintptr_t>& add,
                                                const std::vector<std::uintptr_t>& remove) {
    return calls_.set_outline ? calls_.set_outline(target, add, remove) : RenderingWriteResult{};
}
RenderingWriteResult ActorRendering::Visible(const std::string& key, bool visible) {
    if (key.empty() || key.find('\0') != std::string::npos)
        throw std::invalid_argument("visibility requires a private key");
    return calls_.visible ? calls_.visible(key, visible) : RenderingWriteResult{};
}
bool ActorRendering::Close() {
    return !calls_.close || calls_.close();
}
bool ActorRendering::Current() const {
    return calls_.current && calls_.current();
}
std::optional<std::uint32_t> ActorRendering::ReadMask(const RendererMaskValue& value) const {
    return calls_.read_mask ? calls_.read_mask(value) : std::nullopt;
}
}
