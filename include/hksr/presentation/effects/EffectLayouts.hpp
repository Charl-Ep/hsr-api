#pragma once
#include "EffectControl.hpp"
namespace hksr::api {
enum class EffectLayoutAction { Hide, Show, Perform, Transfer };
enum class EffectLayoutPerform { AddImmediately, RemoveImmediately, SwitchPerform, MaxHighlightPerform };
struct EffectLayoutCommand {
    EffectLayoutAction action = EffectLayoutAction::Hide;
    bool fade = false;
    EffectLayoutPerform perform = EffectLayoutPerform::AddImmediately;
    EffectReference target;
};
struct EffectLayoutServices {
    std::function<EffectControlResult(EffectReference, const EffectLayoutCommand&)> execute;
    std::function<bool()> close;
};
// Immediate operations on caller-owned, retained, already-submitted receipts.
// Transfer is not an undoable setter: callers retain both effects and own their
// later removal. Pool retirement is owned by OwnedEffectRequest, not this service.
class HKSR_API EffectLayouts {
  public:
    explicit EffectLayouts(EffectLayoutServices);
    ~EffectLayouts();
    EffectControlResult Execute(EffectReference, const EffectLayoutCommand&);
    bool Close();

  private:
    EffectLayoutServices services_;
    bool closed_ = false;
};
}
