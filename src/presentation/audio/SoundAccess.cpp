#include <hksr/presentation/audio/SoundAccess.hpp>
#include <hksr/native/Invocation.hpp>
#include <cmath>

namespace hksr::sdk_v1 {
Observed<bool> SoundAccess::EmitterAlive(void* emitter) const {
    Observed<bool> out;
    if(emitter)out.read=native::Call(methods_.alive,&out.value,emitter);
    return out;
}
SoundPostResult SoundAccess::Post(void* manager,void* name,void* emitter,void* ended,void* began) const {
    SoundPostResult out;
    if(!manager || !name || !emitter || !ended || !began)return out;
    const auto alive=EmitterAlive(emitter);
    if(!alive.read.returned || !alive.value)return out; // no listener fallback
    constexpr std::uint32_t end_of_event=1;
    out.attempted=true;
    out.read=native::Call(methods_.post,&out.value,manager,name,emitter,ended,began,end_of_event);
    return out;
}
CallResult SoundAccess::Stop(void* manager,std::uint32_t id,float seconds) const {
    if(!manager || !id || !std::isfinite(seconds) || seconds<0)return {};
    return native::CallVoid(methods_.stop,manager,id,seconds);
}
}
