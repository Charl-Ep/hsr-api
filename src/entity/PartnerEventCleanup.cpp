#include "PartnerEventCleanup.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
struct PartnerEventCleanup::State {
    EntityOwner owner;PartnerEventMethods methods;ManagedObjects& objects;
    ManagedRoots roots;std::vector<ManagedRootId> handles;
    std::map<std::uint64_t,PartnerEventCleanupResult> receipts;
    void* group_type=nullptr;
    unsigned active=0;
    bool closed=false;
    State(const EntityOwner& o,PartnerEventMethods m,ReferenceMethods r,ManagedObjects& obj)
        :owner(o),methods(m),objects(obj),roots(r){}
    void Keep(void* object) {
        const auto index=handles.size();handles.emplace_back();
        const auto handle=roots.Keep(object);handles[index]=handle;
        if(!roots.Status(handle).ready)throw std::runtime_error("partner subscription retention failed");
    }
    void Current(EntityOrigin origin) const {
        const auto live=owner.OwnedToken(origin.token.entity);
        if(!origin.world || !live.entity || live.generation!=origin.token.generation)
            throw std::runtime_error("partner subscription entity no longer owned");
    }
    template<class T>T Read(void* object,std::size_t offset) {
        T value{};
        if(!object || !native::CopyMemoryChecked(&value,static_cast<char*>(object)+offset,sizeof(value)))
            throw std::runtime_error("partner subscription field unreadable");
        return value;
    }
};
PartnerEventCleanup::PartnerEventCleanup(const EntityOwner& o,PartnerEventMethods m,ReferenceMethods r,ManagedObjects& objects)
    :state_(nullptr) {
    if(!m.component.code || !m.owner_world.code || !m.delegate_ctor.code || !m.callback.metadata || !m.unregister.code ||
        m.component_owner<16 || m.world_events<16)throw std::invalid_argument("partner subscription bindings incomplete");
    state_=new State(o,m,r,objects);
}
PartnerEventCleanup::~PartnerEventCleanup(){delete state_;}
PartnerEventCleanupResult PartnerEventCleanup::Remove(EntityOrigin origin) {
    auto& s=*state_;
    if(s.closed)return {false,false,{},"partner subscription cleanup closed"};
    try{s.Current(origin);}catch(const std::exception& e){return {false,false,{},e.what()};}
    // Reserve receipt before any managed operation. Dispose may re-enter; a
    // failed/unknown mutation must not be submitted again for this birth.
    auto [entry,inserted]=s.receipts.try_emplace(origin.token.generation);
    if(!inserted)return entry->second;
    auto& out=entry->second;++s.active;
    struct Guard{unsigned& value;~Guard(){--value;}} guard{s.active};
    try {
        s.Keep(origin.token.entity);s.Keep(origin.world);
        GameWorld* world=nullptr;
        if(!native::Call(s.methods.owner_world,&world,origin.token.entity).returned || world!=origin.world)
            throw std::runtime_error("partner subscription world differs");
        if(!s.group_type) {
            const auto type=s.objects.ManagedType("","OLABFALNLJM");
            if(!type.call.returned || !type.object)throw std::runtime_error("partner group Type unavailable");
            s.Keep(type.object);s.group_type=type.object;
        }
        s.Current(origin);void* group=nullptr;
        if(!native::Call(s.methods.component,&group,origin.token.entity,s.group_type).returned)
            throw std::runtime_error("partner group lookup failed");
        if(group) {
            s.Keep(group);
            if(s.Read<GameEntity*>(group,s.methods.component_owner)!=origin.token.entity)
                throw std::runtime_error("partner group owner differs");
            auto* events=s.Read<void*>(origin.world,s.methods.world_events);
            if(!events)throw std::runtime_error("partner world event manager absent");
            s.Keep(events);
            const auto callback=s.objects.AllocateObject("RPG.GameCore","OnEventCallBack");
            if(!callback.call.returned || !callback.object)throw std::runtime_error("partner callback allocation failed");
            s.Keep(callback.object);s.Current(origin);
            // This game's delegate .ctor consumes MethodInfo, not the native
            // code address. No callback body or parent-death event is executed.
            if(!native::CallVoid(s.methods.delegate_ctor,callback.object,group,s.methods.callback.metadata).returned)
                throw std::runtime_error("partner callback construction failed");
            s.Current(origin);
            out.unregister_attempted=true;
            out.call=native::CallVoid(s.methods.unregister,events,s.methods.event,s.methods.filter,callback.object);
            if(!out.call.returned)throw std::runtime_error("partner event removal failed; outcome may be partial");
        }
        // Auxiliary anchor entities have no group. An already-removed exact
        // callback is also a no-op in the official list removal operation.
        out.returned=true;
    }catch(const std::exception& e){out.error=e.what();}
    return out;
}
bool PartnerEventCleanup::Close() {
    auto& s=*state_;if(s.active)return false;s.closed=true;s.group_type=nullptr;
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(!*it || s.roots.Release(*it).complete){if(*it)s.roots.Forget(*it);it=s.handles.erase(it);}else ++it;
    }
    return s.handles.empty();
}
}
