#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct SoundTrackingLayout {
    std::size_t pending_queue=0,pending_callback=0,pending_emitter=0,pending_banks=0;
    std::size_t playing_callback=0,playing_emitter=0;
};
struct SoundTrackingMethods {
    Method first,next,item,remove,release_banks;
    Method cache,playing_ids,playing_info,count,id_at,stop_playing;
};
struct SoundTrackingBinding {SoundTrackingMethods methods;SoundTrackingLayout layout;};
struct SoundPresence {bool pending=false,playing=false;};
// The fresh completion delegate is the request identity. Logical IDs only
// narrow the playing-cache lookup; they never authorize cancellation.
// Inputs must remain retained on the owning frame. No engine pointers escape.
class HKSR_API SoundTracking {
public:
    SoundTracking(SoundTrackingBinding,ReferenceMethods);
    ~SoundTracking();
    SoundTracking(const SoundTracking&)=delete;
    SoundTracking& operator=(const SoundTracking&)=delete;
    SoundPresence Observe(void* manager,std::uint32_t logical_id,void* callback,void* emitter);
    CallResult Stop(void* manager,std::uint32_t logical_id,void* callback,void* emitter);
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API SoundTrackingBinding ResolveSoundTracking460(RuntimeMetadata&);
}
