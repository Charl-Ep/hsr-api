#include "PresentationCallScope.hpp"
namespace hksr::api::detail {
// One TLS stack in the SDK DLL, not one inline variable per consuming module.
// Native detours and candidate consumers must see the same scope stack.
thread_local PresentationCallScope* PresentationCallScope::current_ = nullptr;
PresentationCallScope::PresentationCallScope(std::optional<PresentationOwner> owner, bool established)
    : previous_(current_), owner_(owner), established_(established) {
    current_ = this;
}
PresentationCallScope::~PresentationCallScope() {
    current_ = previous_;
}
std::optional<PresentationOwner> PresentationCallScope::Current() {
    return current_ && current_->established_ ? current_->owner_ : std::nullopt;
}
bool PresentationCallScope::Established() {
    return current_ && current_->established_;
}
bool PresentationCallScope::Active() {
    return current_ != nullptr;
}
void PresentationCallScope::ExhaustResourceQueries() {
    if (current_)
        current_->resource_queries_ = false;
}
bool PresentationCallScope::ResourceQueriesAllowed() {
    return !current_ || current_->resource_queries_;
}
}
