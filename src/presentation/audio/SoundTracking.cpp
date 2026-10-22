#include <hksr/presentation/audio/SoundTracking.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
#include <vector>

namespace hksr::sdk_v1 {
namespace {
void* Field(void* object,std::size_t offset) {
    void* out=nullptr;
    if(!object || !offset || !native::CopyMemoryChecked(&out,static_cast<char*>(object)+offset,sizeof(out)))
        throw std::runtime_error("audio request field unreadable");
    return out;
}
template<class T,class... Args>T Read(Method method,Args... args) {
    T out{};
    if(!native::Call(method,&out,args...).returned)throw std::runtime_error("audio request lookup failed");
    return out;
}
}
struct SoundTracking::State {
    SoundTrackingBinding binding;ManagedRoots roots;std::vector<ManagedRootId> retained;
    State(SoundTrackingBinding b,ReferenceMethods r):binding(b),roots(r){}
    ManagedRootId Keep(void* object) {
        const auto id=roots.Keep(object);retained.push_back(id);
        if(!roots.Status(id).ready)throw std::runtime_error("audio request lookup retention failed");
        return id;
    }
    bool Release() {
        for(auto it=retained.begin();it!=retained.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=retained.erase(it);}else ++it;
        }
        return retained.empty();
    }
    bool Matches(void* record,void* callback,void* emitter,bool pending) const {
        const auto& l=binding.layout;
        return record && Field(record,pending?l.pending_callback:l.playing_callback)==callback &&
            Field(record,pending?l.pending_emitter:l.playing_emitter)==emitter;
    }
    SoundPresence Visit(void* manager,std::uint32_t id,void* callback,void* emitter,bool stop) {
        if(!manager || !callback || !emitter)throw std::invalid_argument("audio request identity missing");
        const auto& m=binding.methods;const auto& l=binding.layout;
        SoundPresence out;
        auto* queue=Field(manager,l.pending_queue);
        if(!queue)throw std::runtime_error("audio pending queue unavailable");
        auto* node=Read<void*>(m.first,queue);
        while(node) {
            auto* next=Read<void*>(m.next,node);
            auto* value=Read<void*>(m.item,node);
            if(Matches(value,callback,emitter,true)) {
                out.pending=true;
                if(stop) {
                    // Same bank-release / node-removal order as AudioManager,
                    // but selected by the exact delegate rather than ID group.
                    auto* banks=Field(value,l.pending_banks);
                    if(!native::CallVoid(m.release_banks,manager,banks).returned)
                        throw std::runtime_error("audio bank release outcome unknown");
                    if(!native::CallVoid(m.remove,queue,node).returned)
                        throw std::runtime_error("audio pending removal outcome unknown");
                } else return out;
            }
            node=next;
        }
        if(!id) {
            if(!out.pending)throw std::invalid_argument("audio playing lookup requires a logical ID");
            return out;
        }
        auto* cache=Read<void*>(m.cache,manager);
        if(!cache)throw std::runtime_error("audio playing cache unavailable");
        auto* ids=Read<void*>(m.playing_ids,cache,id);
        if(!ids)throw std::runtime_error("audio playing IDs unavailable");
        Keep(ids);
        try {
            const auto count=Read<int>(m.count,ids);
            if(count<0)throw std::runtime_error("audio playing ID count invalid");
            for(int i=0;i<count;++i) {
                const auto playing=Read<std::uint32_t>(m.id_at,ids,i);
                auto* info=Read<void*>(m.playing_info,manager,playing);
                if(!Matches(info,callback,emitter,false))continue;
                out.playing=true;
                // Zero is not a scoped playing capability, even if present in
                // corrupted input. Never pass an all-playing sentinel to Wwise.
                if(stop && (!playing || !native::CallVoid(m.stop_playing,std::uint32_t{0},playing,0).returned))
                    throw std::runtime_error("audio playing stop outcome unknown");
            }
        }catch(...){Release();throw;}
        if(!Release())throw std::runtime_error("audio lookup release incomplete");
        return out;
    }
};
SoundTracking::SoundTracking(SoundTrackingBinding b,ReferenceMethods r):state_(new State(b,r)){}
SoundTracking::~SoundTracking(){delete state_;}
SoundPresence SoundTracking::Observe(void* manager,std::uint32_t id,void* callback,void* emitter) {
    return state_->Visit(manager,id,callback,emitter,false);
}
CallResult SoundTracking::Stop(void* manager,std::uint32_t id,void* callback,void* emitter) {
    state_->Visit(manager,id,callback,emitter,true);return {true,0};
}
bool SoundTracking::Close(){return state_->Release();}
}
