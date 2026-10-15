#include <hksr/world/WorldMap.hpp>
#include <hksr/native/Invocation.hpp>

namespace hksr::sdk_v1 {
WorldMap::WorldMap(WorldMapMethods methods):methods_(std::make_shared<const WorldMapMethods>(std::move(methods))){}
WorldMap::~WorldMap()=default;
WorldMap::WorldMap(const WorldMap&) noexcept=default;
WorldMap& WorldMap::operator=(const WorldMap&) noexcept=default;

namespace {
template<class T> Observed<T> Get(Method method,void* object) {
    Observed<T> out;
    if(object)out.read=native::Call(method,&out.value,object);
    return out;
}
}
WorldMapSnapshot WorldMap::Read(const WorldFrame& frame) const {
    WorldMapSnapshot out;
    // No cross-world fallback and no entity inventory. Disposal observations
    // are independent; never call into a phase already known to be disposing.
    if(!frame.world || !frame.adventure_phase.read.returned ||
       !frame.adventure_phase.value || (frame.disposing.read.returned && frame.disposing.value))return out;
    out.pending_switch=Get<bool>(methods_->pending_switch,frame.adventure_phase.value);
    out.group_manager=Get<void*>(methods_->group_manager,frame.adventure_phase.value);
    if(!out.group_manager.read.returned || !out.group_manager.value)return out;
    out.map=Get<void*>(methods_->map,out.group_manager.value);
    if(!out.map.read.returned || !out.map.value)return out;
    for(std::size_t i=0;i<out.identity.size();++i)
        out.identity[i]=Get<std::uint32_t>(methods_->identity[i],out.map.value);
    out.runtime_id=Get<std::uint64_t>(methods_->runtime_id,out.map.value);
    return out;
}
}
