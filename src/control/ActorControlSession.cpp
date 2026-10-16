#include <hksr/control/ActorControlSession.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/control/ActorControlBindings.hpp>
#include <hksr/model/ArtAdmission.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <map>
#include <hksr/entity/LifecycleHooks.hpp>
#include <cmath>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
struct BusyScope {
    bool& value;
    explicit BusyScope(bool& busy):value(busy) {value=true;}
    ~BusyScope() {value=false;}
};
Rotation4 Multiply(Rotation4 a,Rotation4 b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
using Receipt=std::shared_ptr<ControlResult>;
struct GazeRestoring {};
void Result(const Receipt& receipt,ControlPhase phase,const std::string& detail) {
    if(receipt && !receipt->Terminal()) {receipt->phase=phase;receipt->detail=detail;}
}
}
struct ActorControlSession::State {
    State(EntityToken owner,GameWorld* world,const ActorControlBindings& b)
        :owner_(owner),world_(world),entities_(b.entity),control_(b.control),
        placement_(b.placement),lifetime_(b.lifetime),roots_(b.references),native_(b.lifetime,b.references),arrays_(b.arrays) {}
    bool Ended() const;
    void EnsureActive() const {Require(!closing_ && !Ended(),"control session interrupted");}
    void* Keep(ObjectResult);
    void* Component(GameEntity*,const char*,ManagedObjects&);
    ControlReceipt Place(Position3,Rotation4,bool,std::optional<bool>);
    void CollectPlacement();
    PlayerPlacement PlannedPlacement(PlayerPlacement) const;
    ControlPhase Restore();
    void RequireRestored(const char* message) {
        const auto phase=Restore();
        if(phase==ControlPhase::Waiting)throw GazeRestoring{};
        Require(phase==ControlPhase::Returned,message);
    }
    void ResolveGaze(ActorControlServices);
    void BindGaze(void* mono,void* look,bool direct);
    CallResult StopGaze() {return direct_?control_.StopIk(look_):control_.Stop(mono_);}
    ControlPhase Suspend() {
        if(Ended() || closing_)return ControlPhase::Cancelled;
        if(busy_)return ControlPhase::Waiting;
        BusyScope scope(busy_);
        const auto phase=Restore();
        if(phase!=ControlPhase::Returned) {
            status_.gaze=phase==ControlPhase::Waiting?GazePhase::Restoring:GazePhase::Failed;
            status_.gaze_error=phase==ControlPhase::Waiting?"":"look target restoration failed";
            return phase;
        }
        mono_=look_=target_=nullptr;selection_root_=selection_head_=nullptr;direct_=false;applied_revision_=0;
        status_.gaze=GazePhase::Automatic;status_.gaze_error.clear();return phase;
    }
    void Cancel();
    bool Close(const WorldFrame&);
    void Service(const WorldFrame&,ActorControlServices,GameEntity*,bool);
    EntityToken owner_;GameWorld* world_;
    EntityAccess entities_;ActorControl control_;WorldPlacement placement_;
    UnityLifetimeMethods lifetime_;ManagedRoots roots_;OwnedUnityObjects native_;
    ManagedArrays arrays_;
    std::vector<ManagedRootId> handles_;
    std::map<std::string,void*> types_;
    void* controller_=nullptr;void* driver_=nullptr;void* mono_=nullptr;void* look_=nullptr;
    void* target_=nullptr;void* point_transform_=nullptr;UnityObjectId point_=0;
    void* head_name_=nullptr;
    void* look_type_=nullptr;void* selection_root_=nullptr;void* selection_head_=nullptr;
    std::uint64_t selection_revision_=0;
    void* candidates_=nullptr;
    bool direct_=false;
    bool enabled_gaze_behaviour_=false;
    bool baseline_=false,owns_policy_=false,target_active_=false,closing_=false,busy_=false;
    struct GroundBaseline {bool forbid,only,ground;};
    std::optional<GroundBaseline> ground_baseline_;
    std::uint64_t revision_=0,applied_revision_=0;
    std::uint64_t next_request_=0;
    Receipt gaze_result_,servicing_gaze_;
    std::shared_ptr<PlacementWrite> write_;
    std::vector<Receipt> write_results_;
    struct Pose {Position3 p;Rotation4 q;bool relative;Receipt result;std::optional<bool> snap_to_ground;};
    std::vector<Pose> pending_;
    GazeRequest gaze_;
    ActorControlStatus status_;
};
bool ActorControlSession::State::Ended() const {return TrackedEntities().IsDisposed(owner_) || TrackedEntities().Current(owner_.entity).generation!=owner_.generation;}
void* ActorControlSession::State::Keep(ObjectResult value) {
    EnsureActive();
    Require(value.call.returned && value.object,"control object unavailable");
    const auto root=roots_.Keep(value.object);handles_.push_back(root);EnsureActive();Require(roots_.Status(root).ready,"control reference retention failed");return value.object;
}
void* ActorControlSession::State::Component(GameEntity* entity,const char* name,ManagedObjects& objects) {
    EnsureActive();
    auto& type=types_[name];
    if(!type)type=Keep(objects.ManagedType(name[0]=='.'?"":"RPG.GameCore",name[0]=='.'?name+1:name));
    EnsureActive();const auto value=entities_.Component(entity,type);EnsureActive();Require(value.call.returned,"control component lookup failed");return value.object;
}
ControlReceipt ActorControlSession::State::Place(Position3 p,Rotation4 q,bool relative,std::optional<bool> snap_to_ground) {
    Require(!closing_,"controls closing");
    Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w),"pose must be finite");
    CollectPlacement();
    auto result=std::make_shared<ControlResult>();result->id=++next_request_;
    pending_.push_back({p,q,relative,result,snap_to_ground});status_.placement_pending=true;return result;
}
void ActorControlSession::State::CollectPlacement() {
    if(!write_ || !write_->completed)return;
    const auto phase=write_->call.returned?ControlPhase::Returned:ControlPhase::Failed;
    for(const auto& receipt:write_results_)Result(receipt,phase,write_->call.returned?"official combined placement call returned":"placement call failed");
    status_.placement=write_->call.returned?PlacementPhase::Applied:PlacementPhase::Failed;
    status_.placement_error=write_->call.returned?"":"placement call failed";
    if(write_->call.returned) {
        status_.live.position=write_->position;status_.live.rotation=write_->rotation;status_.live.ready=true;
    }
    write_.reset();write_results_.clear();status_.placement_pending=!pending_.empty();
}
PlayerPlacement ActorControlSession::State::PlannedPlacement(PlayerPlacement fallback) const {
    auto out=status_.live.ready && status_.placement!=PlacementPhase::Failed?status_.live:fallback;
    if(write_ && !write_->cancelled && (!write_->completed || write_->call.returned)) {
        out.position=write_->position;out.rotation=write_->rotation;out.ready=true;
    }
    for(const auto& request:pending_) {
        if(!request.relative) {out.position=request.p;out.rotation=request.q;out.ready=true;}
        else if(out.ready) {
            out.position={out.position.x+request.p.x,out.position.y+request.p.y,out.position.z+request.p.z};
            out.rotation=Multiply(request.q,out.rotation);
        }
    }
    return out;
}
ControlPhase ActorControlSession::State::Restore() {
    if(target_active_) {
        const auto alive=UnityObjectAlive(lifetime_.alive,look_);
        if(!alive.read.returned)return ControlPhase::Failed;
        if(Ended())return ControlPhase::Cancelled;
        if(alive.value && !StopGaze().returned)return ControlPhase::Failed;
        target_active_=false;target_=nullptr;
    }
    if(enabled_gaze_behaviour_) {
        const auto alive=UnityObjectAlive(lifetime_.alive,look_);
        if(!alive.read.returned)return ControlPhase::Failed;
        if(alive.value) {
            // Unity continues the official fade-out. Do not freeze it by
            // restoring a disabled Behaviour while its solver still has weight.
            const auto active=control_.IkActive(look_);
            if(!active.read.returned)return ControlPhase::Failed;
            if(active.value)return ControlPhase::Waiting;
            if(!control_.BehaviourEnabled(look_,false).returned)return ControlPhase::Failed;
        }
        enabled_gaze_behaviour_=false;
    }
    if(owns_policy_) {
        if(Ended())return ControlPhase::Cancelled;
        if(!control_.Automatic(driver_,baseline_).returned)return ControlPhase::Failed;
        owns_policy_=false;
    }
    return ControlPhase::Returned;
}
void ActorControlSession::State::BindGaze(void* mono,void* look,bool direct) {
    if(mono_==mono && look_==look && direct_==direct)return;
    RequireRestored("old look target could not be released");EnsureActive();
    // Retain before publishing the new receiver. A reentrant cancellation
    // leaves no partly published receiver for cleanup to invoke.
    auto* retained=Keep({{true,0},look});
    auto* retained_mono=mono?Keep({{true,0},mono}):nullptr;
    look_=retained;mono_=retained_mono;direct_=direct;target_=nullptr;applied_revision_=0;
}
void ActorControlSession::State::ResolveGaze(ActorControlServices s) {
    OperationSpan timing("sdk.controls.gaze");
    auto mono=control_.Mono(controller_);EnsureActive();
    Require(mono.call.returned,"current MonoAvatar query failed");
    if(mono.object) {
        if(mono_!=mono.object)if(auto* metadata=dynamic_cast<ObjectMetadata*>(&s.objects))control_.BindMono(*metadata,mono.object);
        EnsureActive();const auto look=control_.Look(mono.object);EnsureActive();
        Require(look.call.returned && look.object,"current model has no LookAtIK");
        BindGaze(mono.object,look.object,false);return;
    }
    // Battle Art need not have a CharacterMonoAvatar. The official NPC root
    // can still own a live LookAtIK bound to the CURRENT Art's head. Do not
    // select by name, hierarchy order, proximity or the previous model.
    auto* model=Component(owner_.entity,"CharacterModelComponent",s.objects);
    if(!head_name_)head_name_=Keep(s.objects.NewStringUtf8("Head"));
    const auto head=control_.ModelHead(model,head_name_);EnsureActive();
    Require(head.call.returned && head.object,"current model head unavailable for gaze");
    const auto root=placement_.Root(owner_.entity);EnsureActive();
    Require(root.call.returned && root.object,"current entity root unavailable for gaze");
    if(direct_ && look_) {
        const auto alive=UnityObjectAlive(lifetime_.alive,look_);EnsureActive();
        if(alive.read.returned && alive.value) {
            const auto cached=control_.IkHead(look_);EnsureActive();
            if(cached.call.returned && cached.object==head.object)return;
        }
        RequireRestored("previous gaze binding still in use");EnsureActive();
        look_=mono_=target_=nullptr;direct_=false;
    }
    if(selection_root_!=root.object || selection_head_!=head.object || selection_revision_!=revision_) {
        candidates_=nullptr;selection_root_=Keep(root);selection_head_=Keep(head);selection_revision_=revision_;
    }
    if(!candidates_) {
        if(!look_type_)look_type_=Keep(s.objects.ManagedType("RPG.Client","LookAtIK"));
        const auto list=s.scene.Components(root.object,look_type_,true);EnsureActive();
        candidates_=Keep(list);
    }
    const auto count=arrays_.Length(candidates_);EnsureActive();
    Require(count.read.returned && count.value>=0,"LookAtIK component list unavailable");
    void* match=nullptr;
    for(int i=0;i<count.value;++i) {
        const auto item=arrays_.Get(candidates_,i);EnsureActive();Require(item.call.returned,"LookAtIK list item unreadable");
        if(!item.object)continue;
        const auto alive=UnityObjectAlive(lifetime_.alive,item.object);EnsureActive();
        Require(alive.read.returned,"LookAtIK lifetime unreadable");if(!alive.value)continue;
        const auto binding=control_.IkHead(item.object);EnsureActive();
        Require(binding.call.returned,"LookAtIK head binding unreadable");
        if(binding.object!=head.object)continue;
        Require(!match || match==item.object,"multiple LookAtIK components bind the current head");match=item.object;
    }
    Require(match!=nullptr,"no official LookAtIK bound to current model head");
    BindGaze(nullptr,match,true);
}
void ActorControlSession::State::Cancel() {
    CollectPlacement();closing_=true;
    for(const auto& request:pending_)Result(request.result,ControlPhase::Cancelled,"control session ended before placement");
    for(const auto& receipt:write_results_)Result(receipt,ControlPhase::Cancelled,"control session ended before movement write");
    Result(gaze_result_,ControlPhase::Cancelled,"control session ended before gaze application");
    Result(servicing_gaze_,ControlPhase::Cancelled,"control session interrupted during gaze application");
    pending_.clear();write_results_.clear();if(write_)write_->cancelled=true;status_.placement_pending=false;
}
bool ActorControlSession::State::Close(const WorldFrame& frame) {
    Cancel();
    if(busy_)return false;
    BusyScope scope(busy_);
    if(Ended() && target_active_) {
        const auto alive=UnityObjectAlive(lifetime_.alive,look_);
        if(!alive.read.returned || alive.value)return false; // do not destroy a still-consumed point
        target_active_=false;
    } else if(!Ended()) {
        if(frame.world!=world_ || !frame.disposing.read.returned || frame.disposing.value)return false;
        if(Restore()!=ControlPhase::Returned)return false;
        if(ground_baseline_) {
            if(!control_.ForbidGround(controller_,ground_baseline_->forbid).returned)return false;
            if(!control_.NavigationOnly(controller_,ground_baseline_->only).returned)return false;
            if(!control_.NavigationGround(controller_,ground_baseline_->ground).returned)return false;
            ground_baseline_.reset();
        }
    }
    if(point_ && !native_.Close(point_).complete)return false;
    bool done=true;for(auto id:handles_)if(!roots_.Release(id).complete)done=false;return done;
}
void ActorControlSession::State::Service(const WorldFrame& frame,ActorControlServices s,GameEntity* target,bool ready) {
    OperationSpan timing("sdk.controls.service");
    if(Ended()) {Cancel();return;}
    if(busy_ || closing_ || Ended() || frame.world!=world_ || !frame.disposing.read.returned || frame.disposing.value)return;
    BusyScope scope(busy_);
    const auto entity=entities_.Read(owner_.entity);
    if(closing_ || Ended())return;
    if(!entity.owner_world.read.returned || entity.owner_world.value!=world_ || !entity.loaded.read.returned || !entity.loaded.value)return;
    CollectPlacement();
    status_.live=placement_.ReadEntity(owner_.entity);
    if(!ready)status_.live.ready=false;
    // Entity readiness can precede its Unity root during async loading. Keep
    // the queued pose until that real dependency exists; do not consume it as
    // a failed write or publish the model's temporary origin as saved intent.
    if(!status_.live.ready && !pending_.empty()) {
        status_.placement=PlacementPhase::Waiting;status_.placement_pending=true;
        status_.placement_error.clear();return;
    }
    std::vector<Pose> batch;batch.swap(pending_);
    try {
        EnsureActive();
        if(!batch.empty()) {
            Require(status_.live.ready,"current pose unavailable");
            if(!controller_)controller_=Keep({{true,0},Component(owner_.entity,"AdventureCharacterController",s.objects)});
            auto p=write_?write_->position:status_.live.position;auto q=write_?write_->rotation:status_.live.rotation;
            EnsureActive();
            auto results=write_results_;
            auto snap=write_?write_->snap_to_ground:std::optional<bool>{};
            std::vector<Receipt> replaced;
            for(const auto& request:batch) {
                if(request.snap_to_ground)snap=request.snap_to_ground;
                if(request.relative) {p={p.x+request.p.x,p.y+request.p.y,p.z+request.p.z};q=Multiply(request.q,q);}
                else {
                    p=request.p;q=request.q;
                    replaced.insert(replaced.end(),results.begin(),results.end());
                    results.clear();
                }
                results.push_back(request.result);
            }
            if(snap && !ground_baseline_) {
                const auto original=control_.ForbidGround(controller_);EnsureActive();
                const auto only=control_.NavigationOnly(controller_);EnsureActive();
                const auto ground=control_.NavigationGround(controller_);EnsureActive();
                Require(original.read.returned && only.read.returned && ground.read.returned,"ground/movement policy unavailable");
                ground_baseline_=GroundBaseline{original.value,only.value,ground.value};
            }
            auto next=QueueMovementPlacement(owner_,controller_,p,q,snap);
            if(snap) {
                // The immediate position setter and continuous movement use
                // separate official policies. The direct-motion route keeps
                // animation/navigation deltas but bypasses terrain correction.
                next->navigation_only=*snap?ground_baseline_->only:true;
                next->navigation_ground=*snap?ground_baseline_->ground:false;
            }
            for(const auto& receipt:replaced)Result(receipt,ControlPhase::Superseded,"absolute pose replaced this pending contribution");
            if(write_)write_->cancelled=true;
            write_=std::move(next);write_results_=std::move(results);
            for(const auto& receipt:write_results_)Result(receipt,ControlPhase::Running,"waiting for official movement write");
            status_.placement=PlacementPhase::Waiting;status_.placement_error.clear();
        }
    } catch(const std::exception& e){
        for(const auto& request:batch)Result(request.result,closing_?ControlPhase::Cancelled:ControlPhase::Failed,e.what());
        status_.placement=PlacementPhase::Failed;status_.placement_error=e.what();
    }
    status_.placement_pending=!pending_.empty() || (write_ && !write_->cancelled);
    if(closing_ || Ended())return;
    // Model swaps own their own look policy. Restore before the old model is
    // detached, then rediscover the current Mono/IK at the next ready boundary.
    if(!ready) {
        const auto restored=Restore();
        if(restored==ControlPhase::Returned){mono_=look_=target_=nullptr;selection_root_=selection_head_=nullptr;direct_=false;applied_revision_=0;}
        status_.gaze=restored==ControlPhase::Waiting?GazePhase::Restoring:restored==ControlPhase::Returned?GazePhase::Automatic:GazePhase::Failed;
        status_.gaze_error=restored==ControlPhase::Failed?"look target restoration failed":"";return;
    }
    if(gaze_.mode==GazeMode::Automatic && !owns_policy_ && !enabled_gaze_behaviour_ && !target_active_) {
        status_.gaze=GazePhase::Automatic;status_.gaze_error.clear();
        Result(gaze_result_,ControlPhase::Returned,"automatic gaze policy already active");return;
    }
    const auto gaze=gaze_;const auto revision=revision_;
    const auto receipt=gaze_result_;servicing_gaze_=receipt;
    struct FinishGaze {Receipt& value;~FinishGaze(){value.reset();}} finish_gaze{servicing_gaze_};
    Result(receipt,ControlPhase::Running,"applying official gaze request");
    try {
        if(!controller_)controller_=Keep({{true,0},Component(owner_.entity,"AdventureCharacterController",s.objects)});
        if(gaze.mode==GazeMode::Automatic) {
            RequireRestored("automatic look policy restoration failed");EnsureActive();
            status_.gaze=GazePhase::Automatic;status_.gaze_error.clear();
            Result(receipt,ControlPhase::Returned,"automatic gaze policy restored");return;
        }
        ResolveGaze(s);
        if(!driver_)driver_=Keep({{true,0},Component(owner_.entity,".PKBOEALAKCM",s.objects)});
        if(!owns_policy_) {const auto enabled=control_.Automatic(driver_);EnsureActive();Require(enabled.read.returned,"automatic look policy unavailable");baseline_=enabled.value;owns_policy_=true;Require(control_.Automatic(driver_,false).returned,"automatic target selection pause failed");}
        EnsureActive();
        void* destination=nullptr;
        bool out_of_range=false;
        auto beyond=[&](Position3 target_position) {
            if(gaze.max_distance<=0)return false;
            Require(status_.live.ready,"source position unavailable for gaze distance");
            const double x=double(target_position.x)-status_.live.position.x,y=double(target_position.y)-status_.live.position.y,z=double(target_position.z)-status_.live.position.z;
            return x*x+y*y+z*z>double(gaze.max_distance)*gaze.max_distance;
        };
        if(gaze.mode==GazeMode::Point) {
            if(!point_) {
                const auto object=s.objects.AllocateObject("UnityEngine","GameObject");EnsureActive();Require(object.call.returned && object.object,"look point allocation failed");
                point_=native_.Adopt(object.object);EnsureActive();Require(native_.Status(point_).root.ready,"look point root unavailable");
                Require(s.visuals.Initialize(object.object).returned,"look point initialization failed");
                EnsureActive();
                point_transform_=Keep(s.scene.Transform(object.object));
            }
            if(applied_revision_!=revision)Require(control_.Point(point_transform_,gaze.point).returned,"look point update failed");
            destination=point_transform_;
            out_of_range=beyond(gaze.point);
        } else if(gaze.mode!=GazeMode::Disabled && target) {
            const auto target_state=entities_.Read(target);
            EnsureActive();
            if(target_state.owner_world.read.returned && target_state.owner_world.value==world_ && target_state.loaded.read.returned && target_state.loaded.value) {
                if(gaze.max_distance>0) {
                    const auto pose=placement_.ReadEntity(target);EnsureActive();
                    Require(pose.ready,"target position unavailable for gaze distance");out_of_range=beyond(pose.position);
                }
                // A battle Art can be fully loaded while the adventure Mono
                // is null. Resolve the current model's official attachment,
                // never a retained exploration Mono or a guessed bone path.
                auto* model=Component(target,"CharacterModelComponent",s.objects);
                if(!head_name_)head_name_=Keep(s.objects.NewStringUtf8("Head"));
                EnsureActive();const auto head=control_.ModelHead(model,head_name_);EnsureActive();
                if(head.call.returned)destination=head.object;
            }
        }
        EnsureActive();
        if(out_of_range)destination=nullptr;
        if(destination) {
            if(destination!=target_ || applied_revision_!=revision) {
                if(direct_ && !enabled_gaze_behaviour_) {
                    const auto enabled=control_.BehaviourEnabled(look_);EnsureActive();
                    Require(enabled.read.returned,"official gaze Behaviour state unavailable");
                    if(!enabled.value) {
                        enabled_gaze_behaviour_=true; // record restoration before the setter's side effect
                        target_active_=true; // cancellation during OnEnable must also stop any pending target
                        Require(control_.BehaviourEnabled(look_,true).returned,"official gaze Behaviour could not be enabled");EnsureActive();
                    }
                }
                target_active_=true;Require((direct_?control_.FollowIk(look_,destination):control_.Follow(mono_,destination)).returned,"official head target request failed");target_=destination;
            }
            status_.gaze=GazePhase::Following;status_.gaze_error.clear();
        } else {
            if(target_active_)Require(StopGaze().returned,"official head stop failed");
            target_active_=false;target_=nullptr;
            status_.gaze=out_of_range?GazePhase::OutOfRange:gaze.mode==GazeMode::Disabled?GazePhase::Disabled:GazePhase::TargetUnavailable;status_.gaze_error.clear();
        }
        EnsureActive();applied_revision_=revision;
        if(status_.gaze==GazePhase::TargetUnavailable)
            Result(receipt,revision==revision_?ControlPhase::Waiting:ControlPhase::Superseded,"gaze target unavailable");
        else Result(receipt,ControlPhase::Returned,"official gaze request returned; IK convergence is not asserted");
    } catch(const GazeRestoring&) {
        status_.gaze=GazePhase::Restoring;status_.gaze_error.clear();
        Result(receipt,ControlPhase::Waiting,"waiting for official gaze fade-out");
    } catch(const std::exception& e){status_.gaze=GazePhase::Failed;status_.gaze_error=e.what();Result(receipt,closing_?ControlPhase::Cancelled:ControlPhase::Failed,e.what());}
}

ActorControlSession::ActorControlSession(EntityToken owner,GameWorld* world,const ActorControlBindings& bindings)
    :state_(new State(owner,world,bindings)) {}
ActorControlSession::~ActorControlSession() {state_->Cancel();delete state_;}
ControlReceipt ActorControlSession::Place(Position3 p,Rotation4 q,bool relative,std::optional<bool> snap_to_ground) {return state_->Place(p,q,relative,snap_to_ground);}
ControlReceipt ActorControlSession::Look(GazeRequest request) {
    Require(!state_->closing_,"controls closing");
    Require(std::isfinite(request.max_distance) && request.max_distance>=0,"gaze distance must be finite and nonnegative");
    Require(request.mode!=GazeMode::Point || (std::isfinite(request.point.x) && std::isfinite(request.point.y) && std::isfinite(request.point.z)),"look point must be finite");
    if(state_->gaze_result_!=state_->servicing_gaze_)
        Result(state_->gaze_result_,ControlPhase::Superseded,"new gaze request replaced this pending request");
    auto result=std::make_shared<ControlResult>();result->id=++state_->next_request_;
    state_->gaze_result_=result;state_->gaze_=request;++state_->revision_;return result;
}
void ActorControlSession::Service(const WorldFrame& frame,ActorControlServices services,GameEntity* target,bool ready) {state_->Service(frame,services,target,ready);}
void ActorControlSession::Cancel() {state_->Cancel();}
ControlPhase ActorControlSession::Suspend() {return state_->Suspend();}
bool ActorControlSession::Close(const WorldFrame& frame) {return state_->Close(frame);}
const ActorControlStatus& ActorControlSession::Status() const {return state_->status_;}
PlayerPlacement ActorControlSession::PlannedPlacement(PlayerPlacement fallback) const {return state_->PlannedPlacement(fallback);}
}
