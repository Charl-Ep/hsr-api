#include <hksr/model/ActorModelPresentation.hpp>
#include <cmath>

namespace hksr::api {
struct ActorModelPresentation::State {
    ActorModelPresentationServices services;
    bool closed = false;
    explicit State(ActorModelPresentationServices input) : services(std::move(input)) {
        if (!services.current || !services.close)
            throw std::invalid_argument("model presentation services incomplete");
    }
    void Check() const {
        if (closed || !services.current())
            throw std::runtime_error("model presentation owner ended or replaced");
    }
};
ActorModelPresentation::ActorModelPresentation(ActorModelPresentationServices s)
    : state_(std::make_unique<State>(std::move(s))) {}
ActorModelPresentation::~ActorModelPresentation() = default;
bool ActorModelPresentation::Current() const {
    return !state_->closed && state_->services.current();
}
std::uintptr_t ActorModelPresentation::ModelIdentity() const {
    return state_->services.model_identity;
}
FaceMaterialValue ActorModelPresentation::Face() const {
    auto& s = *state_;
    s.Check();
    if (!s.services.face)
        throw ModelPresentationUnsupported("face material component unavailable");
    return s.services.face();
}
void ActorModelPresentation::SetFace(FaceMaterialTransition value) {
    auto& s = *state_;
    s.Check();
    if (!std::isfinite(value.value.cheek) || !std::isfinite(value.value.shy) || !std::isfinite(value.value.shadow) ||
        !std::isfinite(value.duration) || value.duration < 0)
        throw std::invalid_argument("invalid face material transition");
    if (!s.services.set_face)
        throw ModelPresentationUnsupported("face material component unavailable");
    s.services.set_face(value);
}
void ActorModelPresentation::SetScaleFactor(const std::string& key, float scale) {
    auto& s = *state_;
    s.Check();
    if (key.empty() || key.find('\0') != std::string::npos || !std::isfinite(scale))
        throw std::invalid_argument("invalid model scale factor");
    if (!s.services.set_scale_factor)
        throw ModelPresentationUnsupported("model scale factor unavailable");
    s.services.set_scale_factor(key, scale);
}
float ActorModelPresentation::RootScale() const {
    auto& s = *state_;
    s.Check();
    if (!s.services.root_scale)
        throw ModelPresentationUnsupported("model scale unavailable");
    return s.services.root_scale();
}
EntityOrigin ActorModelPresentation::FollowTarget() const {
    auto& s = *state_;
    s.Check();
    if (!s.services.follow_target)
        throw ModelPresentationUnsupported("model scale follow unavailable");
    return s.services.follow_target();
}
bool ActorModelPresentation::TargetCurrent(EntityOrigin target) const {
    return state_->services.target_current && state_->services.target_current(target);
}
void ActorModelPresentation::Follow(EntityOrigin target) {
    auto& s = *state_;
    s.Check();
    if (!s.services.set_follow)
        throw ModelPresentationUnsupported("model scale follow unavailable");
    if (target.token.entity && !TargetCurrent(target))
        throw std::runtime_error("model scale target ended or is not owned");
    s.services.set_follow(target);
}
bool ActorModelPresentation::Close() {
    auto& s = *state_;
    if (s.closed)
        return true;
    if (s.services.close()) {
        s.closed = true;
        return true;
    }
    return false;
}
}
