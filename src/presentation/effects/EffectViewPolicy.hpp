#pragma once
#include "EffectQueries.hpp"
#include <map>

namespace hksr::sdk_v1::detail {
// Visibility only. The original effect still advances, emits callbacks and
// returns to its pool. Entries are request-generation qualified; no native
// pointer is retained for a later write.
class EffectViewPolicy {
public:
    using Classify=std::function<bool(void*,void*)>;
    using Allow=std::function<bool(TimelineOwner)>;
    using Trace=std::function<void(EffectReference,TimelineOwner,bool)>;
    EffectViewPolicy(Classify classify,Allow allow,Trace trace={}):classify_(std::move(classify)),allow_(std::move(allow)),trace_(std::move(trace)){}
    bool Visible(void* behavior,void* effect,EffectReference request,TimelineOwner owner,bool requested) {
        auto i=entries_.find(behavior);
        if(i==entries_.end() || i->second.effect!=effect || i->second.parameters!=request.parameters ||
            i->second.id!=request.id || !(i->second.owner==owner)) {
            const bool camera=classify_(behavior,effect);
            i=entries_.insert_or_assign(behavior,Entry{effect,request.parameters,request.id,owner,camera}).first;
        }
        const bool visible=requested && (!i->second.camera || allow_(owner));
        if(i->second.camera && (!i->second.observed || i->second.visible!=visible)) {
            i->second.observed=true;i->second.visible=visible;
            if(trace_)try{trace_(request,owner,visible);}catch(...){}
        }
        return visible;
    }
    void Forget(void* behavior){entries_.erase(behavior);}
    static bool CameraSpace(bool ui_effect,int position_follow) {
        // Qualified 4.6 PositionFollowOption: MainCamera, UICamera, UI3DCamera.
        // LocalPlayer and rotation-only LookAtCamera are NOT camera space.
        return ui_effect || position_follow==2 || position_follow==4 || position_follow==6;
    }
private:
    struct Entry {void* effect;void* parameters;std::uint32_t id;TimelineOwner owner;bool camera;bool observed=false,visible=false;};
    Classify classify_;Allow allow_;Trace trace_;std::map<void*,Entry> entries_;
};
// Classification from the loaded effect's own component configuration.
// Does not enumerate scene objects, load assets, or inspect shader names.
HKSR_API EffectViewPolicy::Classify MakeEffectViewClassifier(ObjectMetadata&,TypeRelationships&,ReferenceMethods,
    TimelineRegistryAccess::Fields);
}
