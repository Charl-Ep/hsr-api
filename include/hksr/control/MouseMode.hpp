#pragma once
#include "../managed/References.hpp"
#include <string>
#include <memory>

namespace hksr::sdk_v1 {
class RuntimeMetadata;
struct MouseModeMethods {
    Method instance,force,force_sdk,close,hide,refresh,use_mode,visible,lock;
    std::size_t forced_offset=0;
};
HKSR_API MouseModeMethods ResolveMouseMode(RuntimeMetadata& metadata);
HKSR_API void ObserveMouseModeOwners(const MouseModeMethods& methods);
// A scoped request to the official MouseInputController. External force/close
// requests continue to execute and are preserved when this request is released.
class HKSR_API MouseModeLease {
public:
    MouseModeLease(MouseModeMethods methods,ReferenceMethods references);
    ~MouseModeLease();
    MouseModeLease(const MouseModeLease&)=delete;
    MouseModeLease& operator=(const MouseModeLease&)=delete;
    void Service(bool requested);
    std::string Status() const;
private:
    struct State;
    State* state_;
};
}
