#pragma once
#include "../../managed/References.hpp"

namespace hksr::sdk_v1 {
struct SoundAccessMethods { Method post,stop,alive; };
struct SoundPostResult {std::uint32_t value=0;CallResult read;bool attempted=false;};
// Borrowed, retained inputs on the engine thread. The returned ID belongs to
// AudioManager, not Wwise and not the caller's execution/instance namespace.
class HKSR_API SoundAccess {
public:
    explicit SoundAccess(SoundAccessMethods methods):methods_(methods){}
    Observed<bool> EmitterAlive(void* emitter) const;
    SoundPostResult Post(void* manager,void* name,void* emitter,void* ended,void* began) const;
    // Stops all engine entries sharing this logical ID. A returned call is
    // cancellation submission, not proof that the audio device is silent.
    CallResult Stop(void* manager,std::uint32_t logical_id,float transition_seconds=0) const;
private:
    SoundAccessMethods methods_;
};
HKSR_API SoundAccessMethods ResolveSoundAccess460(MethodResolver&);
}
