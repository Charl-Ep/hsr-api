#include <hksr/model/ModelVisibilityLease.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/ActorInputPolicy.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ModelVisibilityLease::State {
    ManagedObjects& objects;ManagedArrays arrays;ManagedRoots roots;
    std::vector<ManagedRootId> handles;std::unique_ptr<ActorInputPolicy> policy;
    EntityToken owner;void* model=nullptr;void* empty=nullptr;
    bool alternate=false,closing=false;std::string error;
    State(ManagedObjects& o,ReferenceMethods r,ManagedArrays a):objects(o),arrays(std::move(a)),roots(r){}
    void* Keep(ObjectResult result) {
        if(!result.call.returned || !result.object)throw std::runtime_error("weapon visibility input unavailable");
        handles.push_back(0);handles.back()=roots.Keep(result.object);
        if(!roots.Status(handles.back()).ready)throw std::runtime_error("weapon visibility input retention failed");
        return result.object;
    }
};
ModelVisibilityLease::ModelVisibilityLease(MethodResolver& methods,ManagedObjects& objects,ReferenceMethods refs,ManagedArrays arrays)
    :state_(new State(objects,refs,std::move(arrays))) {
    try {
        state_->policy=std::make_unique<ActorInputPolicy>(ActorInputKind::WeaponVisibility,
            methods.Resolve("RPG.GameCore","CharacterModelComponent","_HideWeapons","System.Void",{"System.String[]"}),
            [this](void* receiver,void*)->std::optional<void*> {
                const auto& s=*state_;
                if(s.closing || !s.alternate || receiver!=s.model || !s.empty || !s.owner.entity)return {};
                if(TrackedEntities().IsDisposed(s.owner) || TrackedEntities().Current(s.owner.entity).generation!=s.owner.generation)return {};
                return s.empty;
            });
    }catch(...){delete state_;throw;}
}
ModelVisibilityLease::~ModelVisibilityLease(){if(state_->policy)state_->policy->Stop();delete state_;}
const std::string& ModelVisibilityLease::Error() const{return state_->error;}
bool ModelVisibilityLease::Select(EntityToken owner,void* model,bool alternate) {
    auto& s=*state_;if(s.closing)return false;
    try {
        if(!owner.entity || !model)throw std::invalid_argument("weapon visibility requires an owned model");
        if(alternate && !s.empty) {
            auto* type=s.Keep(s.objects.ManagedType("System","String"));
            s.empty=s.Keep(s.arrays.Create(type,0));
        }
        s.owner=owner;s.model=model;s.alternate=alternate;s.policy->ActivateOnCurrentThread();return true;
    }catch(const std::exception& e){s.error=e.what();return false;}
}
bool ModelVisibilityLease::Close() {
    auto& s=*state_;s.closing=true;s.alternate=false;s.policy->Stop();bool complete=true;
    for(auto id:s.handles)complete=s.roots.Release(id).complete && complete;
    return complete;
}
}
