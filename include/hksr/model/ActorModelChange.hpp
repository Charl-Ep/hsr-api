#pragma once
#include "ModelProfile.hpp"
#include <functional>
#include <string>

namespace hksr::api {
enum class ActorModelChangePhase { Pending, Ready, Unchanged, Failed, Cancelled };
struct ActorModelChangeStatus {
    ActorModelChangePhase phase = ActorModelChangePhase::Pending;
    bool submitted = false, may_have_changed = false, closed = false;
    ChangeResult resolution, change;
    std::string error, cleanup_error;
};
struct ActorModelChangeServices {
    std::function<bool()> current;
    std::function<ChangeResult()> resolve, change;
    std::function<bool()> close;
};
// One official synchronous CanChangeModel/ChangeModel leaf. Ready is its true
// native result on the same owned model component, not an Animator-ready fence.
// Cold assets can stall this Service call. No async preload/callback strategy.
class ActorModelChange {
  public:
    HKSR_API explicit ActorModelChange(ActorModelChangeServices);
    HKSR_API ~ActorModelChange(); // explicit Close; no engine calls
    HKSR_API void Service();
    HKSR_API bool Cancel(); // only before Change submission; never restores
    HKSR_API bool Close();  // GC inputs only, including after partial failure
    HKSR_API const ActorModelChangeStatus& Status() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
