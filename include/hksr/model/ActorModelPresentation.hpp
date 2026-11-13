#pragma once
#include "../entity/EntityLifecycle.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace hksr::api {
struct FaceMaterialValue {
    float cheek = 0, shy = 0, shadow = 0;
    bool operator==(const FaceMaterialValue& b) const {
        return cheek == b.cheek && shy == b.shy && shadow == b.shadow;
    }
};
struct FaceMaterialTransition {
    FaceMaterialValue value;
    float duration = 0;
};
class ModelPresentationUnsupported : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
struct ActorModelPresentationServices {
    std::function<bool()> current;
    std::function<FaceMaterialValue()> face;
    std::function<void(FaceMaterialTransition)> set_face;
    std::function<void(const std::string&, float)> set_scale_factor;
    std::function<float()> root_scale;
    std::function<EntityOrigin()> follow_target;
    std::function<bool(EntityOrigin)> target_current;
    std::function<void(EntityOrigin)> set_follow;
    std::function<bool()> close;
    std::uintptr_t model_identity = 0;
};
// Bound to one owned actor/world/model/Art. Engine-thread calls. Face duration
// is blending time, not a lifetime. Scale factors use caller-owned keys.
// Follow uses the official component's plain entity target branch, not Group,
// attachment, Multiplier, outline or LateUpdate variants. Unsupported is a
// capability result, distinct from an uncertain native write failure.
class ActorModelPresentation {
  public:
    HKSR_API explicit ActorModelPresentation(ActorModelPresentationServices);
    HKSR_API ~ActorModelPresentation(); // no engine calls
    HKSR_API bool Current() const;
    HKSR_API std::uintptr_t ModelIdentity() const;
    HKSR_API FaceMaterialValue Face() const;
    HKSR_API void SetFace(FaceMaterialTransition);
    HKSR_API void SetScaleFactor(const std::string& key, float scale);
    HKSR_API float RootScale() const;
    HKSR_API EntityOrigin FollowTarget() const;
    HKSR_API bool TargetCurrent(EntityOrigin) const;
    HKSR_API void Follow(EntityOrigin); // empty target stops this driver's contribution
    HKSR_API bool Close();              // GC inputs only; consumers withdraw their own writes first
  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
