#include "CommonTimelinePlayback.hpp"
#include "TimelineBorrowCapture.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/entity/EntityProvenance.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/world/WorldPlacement.hpp>
#include <algorithm>

namespace hksr::sdk_v1::detail {
namespace {
TimelineTicket Ticket(TimelineReference ref) {
    return {{reinterpret_cast<std::uintptr_t>(ref.registry),reinterpret_cast<std::uintptr_t>(ref.record)},ref.playback,ref.borrow};
}
TimelineOwner Owner(EntityOrigin origin) {
    return {reinterpret_cast<std::uintptr_t>(origin.world),reinterpret_cast<std::uintptr_t>(origin.token.entity),origin.token.generation};
}
bool Alive(EntityOrigin origin) {
    return origin.world && origin.token.entity && !TrackedEntities().IsDisposed(origin.token) &&
        TrackedEntities().Current(origin.token.entity).generation==origin.token.generation;
}
}
struct CommonTimelinePlayback::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    TimelineRegistryTracking& tracking;ManagedRoots roots;EntityAccess entities;
    Method play,stop,field_get;void* manager_field=nullptr;
    std::vector<ManagedRootId> held;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,TimelineRegistryTracking& t)
        :methods(m),metadata(d),objects(o),tracking(t),roots(r),entities(ResolveEntityAccessMethods(m)) {
        play=m.Resolve("","NCEDGGKOMIH","NOLKCBFDAOJ","System.Int32",{
            "System.UInt32","System.String","UnityEngine.Playables.PlayableAsset","UnityEngine.Vector3","UnityEngine.Vector3",
            "System.Action<UnityEngine.Playables.PlayableDirector,UnityEngine.Playables.PlayableAsset>",
            "UnityEngine.Events.UnityAction","System.Double","UnityEngine.Playables.DirectorWrapMode"});
        stop=m.Resolve("","EBMMGODFEEO","HFKFNFKIEON","System.Void",{});
    }
    ManagedRootId Keep(void* object) {
        if(!object)throw std::runtime_error("Timeline input is null");
        held.reserve(held.size()+1);const auto root=roots.Keep(object);held.push_back(root);
        if(!roots.Status(root).ready)throw std::runtime_error("Timeline input retention failed");
        return root;
    }
    void Release(ManagedRootId root) {
        if(roots.Release(root).complete){roots.Forget(root);held.erase(std::remove(held.begin(),held.end(),root),held.end());}
    }
    void* Object(ObjectResult value) {
        if(!value.call.returned || !value.object)throw std::runtime_error("Timeline metadata object unavailable");
        Keep(value.object);return value.object;
    }
    void* Manager() {
        if(!manager_field) {
            auto* type=Object(objects.ManagedType("RPG.Client","GlobalVars"));
            auto* name=Object(objects.NewStringUtf8("s_TimelineManager"));
            const auto get=methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"});
            ObjectResult result;result.call=native::Call(get,&result.object,type,name,56);
            auto* field=Object(result);const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
            field_get=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetValue","System.Object",{"System.Object"});
            manager_field=field;
        }
        ObjectResult manager;manager.call=native::Call(field_get,&manager.object,manager_field,static_cast<void*>(nullptr));
        if(!manager.call.returned || !manager.object)throw std::runtime_error("official Timeline manager unavailable");
        return manager.object;
    }
};
CommonTimelinePlayback::CommonTimelinePlayback(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,TimelineRegistryTracking& t)
    :state_(std::make_unique<State>(m,d,o,r,t)){}
CommonTimelinePlayback::~CommonTimelinePlayback() {
    // Process-shared service. Never release managed roots from native teardown.
    if(!state_->held.empty())state_.release();
}
bool CommonTimelinePlayback::Close() {
    auto& s=*state_;const auto held=s.held;
    for(auto i=held.rbegin();i!=held.rend();++i)s.Release(*i);
    s.manager_field=nullptr;return s.held.empty();
}
TimelineStartResult CommonTimelinePlayback::Start(EntityOrigin origin,const std::string& path,int wrap,double time) {
    auto& s=*state_;TimelineStartResult out;out.attributed=true;std::vector<ManagedRootId> inputs;
    const auto hold=[&](void* object){inputs.push_back(s.Keep(object));};
    const auto release=[&]{for(auto i=inputs.rbegin();i!=inputs.rend();++i)s.Release(*i);};
    try {
        if(!Alive(origin))throw std::runtime_error("Timeline source entity retired");
        hold(origin.world);hold(origin.token.entity);
        const auto entity=s.entities.Read(origin.token.entity);
        if(!Alive(origin) || !entity.owner_world.read.returned || entity.owner_world.value!=origin.world || !entity.runtime_id.read.returned)
            throw std::runtime_error("Timeline source world or RuntimeID unavailable");
        auto* manager=s.Manager();hold(manager);
        const auto named=s.objects.NewStringUtf8(path.c_str());
        if(!named.call.returned || !named.object)throw std::runtime_error("Timeline path allocation failed");
        hold(named.object);
        if(!Alive(origin))throw std::runtime_error("Timeline source ended during preparation");
        TimelineCallScope scope(Owner(origin));TimelineBorrowCapture capture(Owner(origin));
        Position3 position{},rotation{};
        // Common's explicit-asset and binding callback are null. The SDK owns
        // its result through the registry, not a PlayTimeline task's lifecycle
        // or its global completion/volume notification callback.
        out.attributed=false;
        out.call=native::Call(s.play,&out.playback,manager,entity.runtime_id.value,named.object,
            static_cast<void*>(nullptr),&position,&rotation,static_cast<void*>(nullptr),static_cast<void*>(nullptr),time,wrap);
        for(const auto& ticket:capture.tickets)out.references.push_back({reinterpret_cast<void*>(ticket.key.registry),
            reinterpret_cast<void*>(ticket.key.record),ticket.playback,ticket.borrow});
        out.attributed=capture.entered==capture.tickets.size();
        if(out.call.returned && std::none_of(out.references.begin(),out.references.end(),[&](const auto& ref){return ref.playback==out.playback;})) {
            out.attributed=false;out.detail="Timeline returned without an attributed playback";
        }else if(!out.attributed)out.detail="Timeline borrow did not produce a complete ownership receipt";
        else if(!out.call.returned)out.detail="Timeline start did not return normally; captured borrows retained";
        release();return out;
    }catch(const std::exception& e){out.detail=e.what();release();return out;}
}
TimelinePlaybackPhase CommonTimelinePlayback::Observe(TimelineOwner owner,TimelineReference ref) {
    auto& s=*state_;const auto ticket=Ticket(ref);const auto& ownership=s.tracking.Ownership();
    if(!ownership.Pending(ticket))return TimelinePlaybackPhase::Retired;
    if(!(ownership.Owner(ticket.key)==owner))return TimelinePlaybackPhase::Unresolved;
    const auto row=s.tracking.ObserveRecord(ref.record);
    // Reading may allocate/dispatch. Check the borrow again before using it.
    if(!ownership.Pending(ticket))return TimelinePlaybackPhase::Retired;
    if(!row || row->key.registry!=ticket.key.registry || row->playback!=ticket.playback)return TimelinePlaybackPhase::Unresolved;
    switch(row->phase) {
    case TimelinePhase::Preparing:return TimelinePlaybackPhase::Preparing;
    case TimelinePhase::Playing:return TimelinePlaybackPhase::Playing;
    case TimelinePhase::Paused:return TimelinePlaybackPhase::Paused;
    case TimelinePhase::Stopped:return TimelinePlaybackPhase::Stopped;
    default:return TimelinePlaybackPhase::Unresolved;
    }
}
CallResult CommonTimelinePlayback::Stop(TimelineOwner owner,TimelineReference ref) {
    const auto phase=Observe(owner,ref);
    if(phase==TimelinePlaybackPhase::Retired)return {true,0};
    if(phase!=TimelinePlaybackPhase::Playing && phase!=TimelinePlaybackPhase::Paused)return {};
    TimelineCallScope scope(owner);
    return native::CallVoid(state_->stop,ref.record);
}
}
