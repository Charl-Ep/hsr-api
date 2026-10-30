#include <hksr/presentation/movement/AnimationMovementAccess.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>

namespace hksr::sdk_v1 {
namespace {
template<class T>bool Read(void* object,std::size_t offset,T& value) {
    return object && native::CopyMemoryChecked(&value,static_cast<char*>(object)+offset,sizeof(value));
}
MovementWriteState Ownership(const AnimationMovementObservation& observed,void* expected) {
    if(!observed.read.returned)return MovementWriteState::Failed;
    if(!observed.data)return MovementWriteState::Detached;
    return observed.data==expected?MovementWriteState::Owned:MovementWriteState::Replaced;
}
}
AnimationMovementObservation AnimationMovementAccess::Observe(void* component)const {
    AnimationMovementObservation out;const auto& b=binding_;
    out.read.returned=Read(component,b.owner,out.owner)&&Read(component,b.transform,out.transform)&&
        Read(component,b.animator_source,out.animator_source)&&Read(component,b.other_motion,out.other_motion)&&
        Read(component,b.data,out.data)&&Read(component,b.attached,out.attached)&&Read(component,b.active,out.active)&&
        Read(component,b.index,out.segment_index);
    return out;
}
AnimationMoveClockObservation AnimationMovementAccess::Clock(const AnimationMovementData& data)const {
    AnimationMoveClockObservation out;auto* object=data.Object();const auto& b=binding_.graph;
    out.read.returned=Read(object,b.key,out.key)&&Read(object,b.previous_time,out.animation_time)&&Read(object,b.progress,out.movement_time);
    return out;
}
MovementWriteResult AnimationMovementAccess::Submit(void* component,const AnimationMovementData& data,
    const std::function<bool()>& current)const {
    MovementWriteResult out;auto* object=data.Object();
    if(!object || !component || !current || !current()){out.error="movement owner/model is not current";return out;}
    out.before=Observe(component);
    if(!out.before.read.returned){out.error="movement component unreadable";return out;}
    if(!owner_ || out.before.owner!=owner_){out.error="movement component belongs to a different entity";return out;}
    if(!out.before.owner || !out.before.transform || !out.before.animator_source){out.error="movement component is not initialized";return out;}
    if(out.before.data || out.before.other_motion || out.before.attached) {
        out.error="movement channel has another owner";return out;
    }
    // The official entry exits other motion BEFORE inspecting FGNN's key.
    // Empty-channel ownership must therefore be established before the call.
    if(!binding_.submit.code){out.error="movement submission binding unavailable";return out;}
    if(!data.BeginSubmission()){out.error="movement graph already submitted; prepare a new request";return out;}
    out.attempted=true;out.call=native::CallVoid(binding_.submit,component,object);
    if(!out.call.returned){out.state=MovementWriteState::Failed;out.error="movement submission did not return";return out;}
    if(!current()){out.state=MovementWriteState::Failed;out.error="movement owner retired during submission";return out;}
    out.after=Observe(component);out.state=Ownership(out.after,object);
    if(out.after.read.returned && out.after.owner!=owner_){out.state=MovementWriteState::Failed;out.error="movement component owner changed during submission";return out;}
    if(out.state==MovementWriteState::Failed)out.error="movement adoption unreadable";
    return out;
}
MovementWriteResult AnimationMovementAccess::Position(void* component,Position3 position,
    const std::function<bool()>& current)const {
    MovementWriteResult out;
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)) {
        out.error="movement position must be finite";return out;
    }
    if(!component || !current || !current()){out.error="movement owner/model is not current";return out;}
    out.before=Observe(component);
    if(!out.before.read.returned){out.error="movement component unreadable";return out;}
    if(!owner_ || out.before.owner!=owner_){out.error="movement component belongs to a different entity";return out;}
    if(!out.before.transform || !out.before.animator_source){out.error="movement component is not initialized";return out;}
    // CGKHLCOPDIF flushes pending motion before setting TransformComponent.WorldPos.
    // The lease drains Adventure motion; never cancel a foreign channel here.
    if(out.before.data || out.before.other_motion || out.before.attached){out.error="movement channel has another owner";return out;}
    if(!binding_.position.code){out.error="movement positioning binding unavailable";return out;}
    out.attempted=true;out.call=native::CallVoid(binding_.position,component,position);
    if(!out.call.returned){out.state=MovementWriteState::Failed;out.error="movement positioning did not return";return out;}
    if(!current()){out.state=MovementWriteState::Failed;out.error="movement owner retired during positioning";return out;}
    out.after=Observe(component);
    if(!out.after.read.returned || out.after.owner!=owner_){out.state=MovementWriteState::Failed;out.error="movement position ownership unreadable or changed";return out;}
    out.state=MovementWriteState::Detached;return out;
}
MovementWriteResult AnimationMovementAccess::Interrupt(void* component,const AnimationMovementData& data,
    const std::function<bool()>& current)const {
    MovementWriteResult out;auto* object=data.Object();
    if(!object || !component || !current || !current()){out.error="movement owner/model is not current";return out;}
    out.before=Observe(component);
    if(!out.before.read.returned){out.state=MovementWriteState::Failed;out.error="movement ownership unreadable";return out;}
    if(!owner_ || out.before.owner!=owner_){out.error="movement component belongs to a different entity";return out;}
    out.state=Ownership(out.before,object);
    if(out.state!=MovementWriteState::Owned) {
        if(out.state==MovementWriteState::Failed)out.error="movement ownership unreadable";
        return out; // another graph, including one with the same animation key
    }
    if(!binding_.clear.code){out.state=MovementWriteState::NotAttempted;out.error="movement interruption binding unavailable";return out;}
    out.attempted=true;out.call=native::CallVoid(binding_.clear,component);
    if(!out.call.returned){out.state=MovementWriteState::Failed;out.error="movement interruption did not return";return out;}
    if(!current()){out.state=MovementWriteState::Failed;out.error="movement owner retired during interruption";return out;}
    out.after=Observe(component);out.state=Ownership(out.after,object);
    if(out.after.read.returned && out.after.owner!=owner_){out.state=MovementWriteState::Failed;out.error="movement component owner changed during interruption";return out;}
    if(out.state==MovementWriteState::Owned || out.state==MovementWriteState::Failed)out.error="movement detachment not observed";
    return out;
}
}
