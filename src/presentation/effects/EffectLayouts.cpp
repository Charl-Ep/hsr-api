#include <hksr/presentation/effects/EffectLayouts.hpp>
#include <utility>
namespace hksr::api {
EffectLayouts::EffectLayouts(EffectLayoutServices services) : services_(std::move(services)) {}
EffectLayouts::~EffectLayouts() = default;
EffectControlResult EffectLayouts::Execute(EffectReference ref, const EffectLayoutCommand& command) {
    if (closed_)
        return {EffectControlState::Ended, {}, true, "effect layout service closed"};
    if (!ref.id || !ref.parameters)
        return {EffectControlState::Failed, {}, true, "layout requires an exact submitted effect receipt"};
    if (static_cast<int>(command.action) < 0 || static_cast<int>(command.action) > 3)
        return {EffectControlState::Failed, {}, true, "unknown layout operation"};
    if (command.action == EffectLayoutAction::Transfer &&
        (!command.target.id || !command.target.parameters ||
         (ref.id == command.target.id && ref.parameters == command.target.parameters)))
        return {EffectControlState::Failed, {}, true, "layout transfer requires two distinct receipts"};
    const auto type = static_cast<int>(command.perform);
    if (type < 0 || type > 3)
        return {EffectControlState::Failed, {}, true, "unknown layout perform operation"};
    return services_.execute
               ? services_.execute(ref, command)
               : EffectControlResult{EffectControlState::Unsupported, {}, true, "effect layout service unavailable"};
}
bool EffectLayouts::Close() {
    if (closed_)
        return true;
    if (services_.close && !services_.close())
        return false;
    closed_ = true;
    return true;
}
}
