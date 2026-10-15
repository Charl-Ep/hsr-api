#include <hksr/control/ActorControl.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/native/Invocation.hpp>
#include "../platform/NativeHooks.hpp"
#include <mutex>
#include <cmath>
#include <stdexcept>

namespace hksr::sdk_v1 {
ActorControl::ActorControl(ActorControlMethods methods):methods_(std::make_shared<const ActorControlMethods>(std::move(methods))){}
ActorControl::~ActorControl()=default;
ActorControl::ActorControl(const ActorControl&) noexcept=default;
ActorControl& ActorControl::operator=(const ActorControl&) noexcept=default;

namespace {
detail::NativeHooks hooks;ActorControlMethods movement;
std::mutex pending_mutex;std::vector<std::weak_ptr<PlacementWrite>> pending;
std::atomic<bool> has_pending{false};
void __fastcall Flush(void* self,void* method) {
    reinterpret_cast<void(__fastcall*)(void*,void*)>(hooks.Original(0))(self,method);
    if(!has_pending.load(std::memory_order_relaxed))return;
    try {
    std::vector<std::shared_ptr<PlacementWrite>> batch;
    {std::lock_guard<std::mutex> lock(pending_mutex);
        for(auto it=pending.begin();it!=pending.end();) {
            auto request=it->lock();
            if(!request || request->cancelled) {it=pending.erase(it);continue;}
            if(request->controller==self && request->thread==GetCurrentThreadId()) {batch.push_back(request);it=pending.erase(it);}
            else ++it;
        }
        has_pending.store(!pending.empty(),std::memory_order_relaxed);
    }
    for(auto& request:batch) {
        if(request->cancelled) continue;
        const auto current=TrackedEntities().Current(request->owner.entity);
        if(current.generation==request->owner.generation && !TrackedEntities().IsDisposed(request->owner)) {
            // The official combined setter itself evaluates ground projection.
            // Change the policy before that call, at the same movement boundary.
            if(request->snap_to_ground) {
                request->call=native::CallVoid(movement.forbid_ground,self,!*request->snap_to_ground);
                if(!request->call.returned){request->completed=true;continue;}
            }
            if(request->navigation_only) {
                request->call=native::CallVoid(movement.navigation_only,self,*request->navigation_only);
                if(!request->call.returned){request->completed=true;continue;}
            }
            if(request->navigation_ground) {
                request->call=native::CallVoid(movement.navigation_ground,self,*request->navigation_ground);
                if(!request->call.returned){request->completed=true;continue;}
            }
            request->call=native::CallVoid(movement.place,self,&request->position,&request->rotation);
        }
        request->completed=true;
    }
    } catch(...) {} // never unwind our consumer through the completed game call
}
}
void InstallMovementPlacement(const ActorControlMethods& methods) {
    if(hooks.Pending()) throw std::logic_error("movement placement already installed");
    if(!methods.flush.code || !methods.place.code) throw std::runtime_error("movement placement methods unavailable");
    movement=methods;hooks.Install({{"AdventureCharacterController.FlushMovementThreadData",methods.flush.code,reinterpret_cast<void*>(&Flush)}});
}
void RemoveMovementPlacementAtQuiescentPoint() {hooks.Remove();std::lock_guard<std::mutex> lock(pending_mutex);pending.clear();has_pending=false;movement={};}
std::shared_ptr<PlacementWrite> QueueMovementPlacement(EntityToken owner,void* controller,Position3 position,Rotation4 rotation,std::optional<bool> snap_to_ground) {
    if(!hooks.Pending() || !controller || !owner.entity || !owner.generation) throw std::runtime_error("movement placement boundary unavailable");
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)||!std::isfinite(rotation.x)||!std::isfinite(rotation.y)||!std::isfinite(rotation.z)||!std::isfinite(rotation.w))
        throw std::invalid_argument("placement must be finite");
    auto request=std::make_shared<PlacementWrite>();request->owner=owner;request->controller=controller;request->position=position;request->rotation=rotation;request->thread=GetCurrentThreadId();request->snap_to_ground=snap_to_ground;
    std::lock_guard<std::mutex> lock(pending_mutex);pending.push_back(request);has_pending=true;return request;
}
ObjectResult ActorControl::Mono(void* value) const {ObjectResult out;if(value)out.call=native::Call(methods_->mono,&out.object,value);return out;}
ObjectResult ActorControl::Head(void* value) const {ObjectResult out;if(value)out.call=native::Call(methods_->head,&out.object,value);return out;}
ObjectResult ActorControl::ModelHead(void* model,void* name) const {ObjectResult out;if(model && name)out.call=native::Call(methods_->model_attach_point,&out.object,model,name,false);return out;}
ObjectResult ActorControl::Look(void* value) const {ObjectResult out;if(value)out.call=native::Call(methods_->look,&out.object,value);return out;}
ObjectResult ActorControl::IkHead(void* value) const {ObjectResult out;if(value)out.call=native::Call(methods_->ik_head,&out.object,value);return out;}
CallResult ActorControl::FollowIk(void* value,void* target) const {return value&&target?native::CallVoid(methods_->ik_follow,value,target,true,false,static_cast<void*>(nullptr)):CallResult{};}
CallResult ActorControl::StopIk(void* value) const {return value?native::CallVoid(methods_->ik_stop,value,false):CallResult{};}
Observed<bool> ActorControl::IkActive(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->ik_active,&out.value,value);return out;}
Observed<bool> ActorControl::BehaviourEnabled(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->behaviour_enabled,&out.value,value);return out;}
CallResult ActorControl::BehaviourEnabled(void* value,bool enabled) const {return value?native::CallVoid(methods_->set_behaviour_enabled,value,enabled):CallResult{};}
Observed<bool> ActorControl::Automatic(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->look_enabled,&out.value,value);return out;}
Observed<bool> ActorControl::ForbidGround(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->get_forbid_ground,&out.value,value);return out;}
CallResult ActorControl::ForbidGround(void* value,bool enabled) const {return value?native::CallVoid(methods_->forbid_ground,value,enabled):CallResult{};}
Observed<bool> ActorControl::NavigationOnly(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->get_navigation_only,&out.value,value);return out;}
CallResult ActorControl::NavigationOnly(void* value,bool enabled) const {return value?native::CallVoid(methods_->navigation_only,value,enabled):CallResult{};}
Observed<bool> ActorControl::NavigationGround(void* value) const {Observed<bool> out;if(value)out.read=native::Call(methods_->get_navigation_ground,&out.value,value);return out;}
CallResult ActorControl::NavigationGround(void* value,bool enabled) const {return value?native::CallVoid(methods_->navigation_ground,value,enabled):CallResult{};}
CallResult ActorControl::Automatic(void* value,bool enabled) const {return value?native::CallVoid(methods_->set_look_enabled,value,enabled):CallResult{};}
CallResult ActorControl::Follow(void* value,void* target) const {return value&&target?native::CallVoid(methods_->follow,value,target,true,false,static_cast<void*>(nullptr)):CallResult{};}
CallResult ActorControl::Stop(void* value) const {return value?native::CallVoid(methods_->stop,value,false):CallResult{};}
CallResult ActorControl::Point(void* value,Position3 point) const {return value?native::CallVoid(methods_->point,value,&point):CallResult{};}
void ActorControl::BindMono(ObjectMetadata& metadata,void* mono) {
    const auto type=metadata.ReadObjectType(mono);MetadataResolver resolver({type});
    auto updated=*methods_;
    // Virtual getters may be abstract on CharacterMonoAvatar. Prefer this
    // actual model's implementation; do not call an abstract base slot.
    auto get=[&](Method& method,const char* name,const char* result) {
        bool declared=false;for(const auto& entry:type.methods)if(entry.name==name && entry.parameters.empty())declared=true;
        if(declared)method=resolver.Resolve(type.name_space.c_str(),type.name.c_str(),name,result,{});
    };
    get(updated.head,"get_Head","UnityEngine.Transform");get(updated.look,"get_LookAtIK","RPG.Client.LookAtIK");
    methods_=std::make_shared<const ActorControlMethods>(std::move(updated));
}
}
