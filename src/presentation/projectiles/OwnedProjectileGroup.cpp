#include <hksr/presentation/projectiles/OwnedProjectileGroup.hpp>
#include "ProjectileWrapperCapture.hpp"
#include <hksr/presentation/projectiles/ProjectilePhysics.hpp>
#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
void Require(CallResult r,const char* operation) {
    if(!r.returned)throw std::runtime_error(std::string(operation)+": exception="+std::to_string(r.exception));
}
template<class T>T Read(void* p,std::size_t offset) {
    T out{};
    if(!p || !native::CopyMemoryChecked(&out,static_cast<char*>(p)+offset,sizeof(out)))throw std::runtime_error("projectile field unreadable");
    return out;
}
template<class T>void WritePrivate(void* p,std::size_t offset,const T& value) {
    if(!p || !native::CopyMemoryChecked(static_cast<char*>(p)+offset,&value,sizeof(value)))throw std::runtime_error("private projectile field write failed");
}
std::size_t Field(const ProjectileGroupServices& s,const char* ns,const char* type,const char* name,const char* expected) {
    std::optional<std::size_t> found;
    for(const auto& f:s.fields(ns,type))if(f.name==name) {
        if(found || !f.flags_observed || (f.flags&0x10) || f.type!=expected)throw std::runtime_error(std::string("projectile field differs: ")+name);
        found=f.offset;
    }
    if(!found)throw std::runtime_error(std::string("projectile field absent: ")+name);
    return *found;
}
}
struct OwnedProjectileGroup::State {
    ProjectileGroupServices services;EntityOrigin owner;ManagedRoots roots;ManagedArrays arrays;
    std::vector<ManagedRootId> held;std::set<void*> retained;
    ProjectileGroupStatus status;
    Method component_ctor,owner_ref,config_ctor,ability_ctor,ability_dispose,context_get,skill_ctor,list_get,list_add,list_contains,tick;
    Method carrier_ctor,carrier_dispose,partial_cleanup,previous_set;
    std::size_t wrapper=0,position=0,velocity=0,phase=0,ordered=0,last_position=0,last_velocity=0,owner_kind=0;
    ProjectilePhysicsMethods physics_methods;
    void* component=nullptr;void* ability=nullptr;void* skill=nullptr;void* list=nullptr;
    bool attempted=false,ability_constructed=false,dispose_attempted=false,disposed=false,busy=false;
    EntityDisposalSubscription owner_subscription=0;
    struct Flight {
        ManagedRoots roots;std::vector<ManagedRootId> held;std::set<void*> retained;
        explicit Flight(ReferenceMethods r):roots(r){}
        void* Keep(void* value) {
            if(!value)throw std::runtime_error("projectile flight input absent");
            if(retained.count(value))return value;
            held.reserve(held.size()+1);held.push_back(roots.Keep(value));
            if(!roots.Status(held.back()).ready)throw std::runtime_error("projectile flight retention failed");
            retained.insert(value);return value;
        }
        bool Release() {
            for(auto it=held.begin();it!=held.end();) {
                if(roots.Release(*it).complete){roots.Forget(*it);it=held.erase(it);}else ++it;
            }
            return held.empty();
        }
        ProjectileId id=0;EntityOrigin target;std::function<bool()> current;
        EntityDisposalSubscription subscription=0;
        std::unique_ptr<ProjectileInputs> inputs;
        std::unique_ptr<ProjectilePhysics> physics;
        std::unique_ptr<detail::ProjectileWrapperCapture> wrappers;
        void* carrier=nullptr;
        bool constructed=false,listed=false,arrived=false,finished=false,cleanup_attempted=false;
        bool capture_failed=false;
        std::vector<std::unique_ptr<OwnedEffectRequest>> effects;
    };
    std::vector<std::unique_ptr<Flight>> flights;
    ProjectileId next_id=0;
    Flight* submitting=nullptr;
    std::unique_ptr<EffectSubmissions> submissions;
    State(ProjectileGroupServices s,EntityOrigin o):services(std::move(s)),owner(o),roots(services.references),arrays(services.arrays){}
    void Error(const std::string& error) {
        if(std::find(status.errors.begin(),status.errors.end(),error)==status.errors.end()) {
            status.errors.push_back(error);if(services.trace)try{services.trace(error);}catch(...){}
        }
    }
    void* Keep(void* p) {
        if(!p)throw std::runtime_error("projectile managed object absent");
        if(retained.count(p))return p;
        held.reserve(held.size()+1);held.push_back(roots.Keep(p));
        if(!roots.Status(held.back()).ready)throw std::runtime_error("projectile managed retention failed");
        retained.insert(p);return p;
    }
    void* New(const char* ns,const char* name) {
        const auto r=services.objects->AllocateObject(ns,name);Require(r.call,"projectile allocation");return Keep(r.object);
    }
    template<class... A>void* Object(Method m,A... args) {
        ObjectResult r;r.call=native::Call(m,&r.object,args...);Require(r.call,"projectile object call");return Keep(r.object);
    }
    void Reference(void* receiver,const char* ns,const char* type,const char* name,const char* expected,void* value) {
        Field(services,ns,type,name,expected);
        const auto t=services.objects->ManagedType(ns,type),n=services.objects->NewStringUtf8(name);
        Require(t.call,"projectile field Type");Keep(t.object);Require(n.call,"projectile field name");Keep(n.object);
        auto* f=Object(services.methods->Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"}),t.object,n.object,54);
        const auto actual=services.metadata->ReadObjectType(f);MetadataResolver methods({actual});
        const auto set=methods.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
        Require(native::CallVoid(set,f,receiver,value,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)),"private projectile reference");
    }
    bool Current()const {
        return services.owner_current && services.owner_current() && !services.lifecycle->IsDisposed(owner.token);
    }
    void Bind() {
        auto& m=*services.methods;
        component_ctor=m.Resolve("RPG.GameCore","AbilityComponent",".ctor","System.Void",{});
        owner_ref=m.Resolve("RPG.GameCore","GameComponentBase","InitOwnerRef","System.Void",{"RPG.GameCore.GameEntity"});
        config_ctor=m.Resolve("RPG.GameCore","AdventureAbilityConfig",".ctor","System.Void",{});
        ability_ctor=m.Resolve("","AEEKMHCMBPL",".ctor","System.Void",{"RPG.GameCore.AbilityComponent","RPG.GameCore.AdventureAbilityConfig"});
        ability_dispose=m.Resolve("","AEEKMHCMBPL","Dispose","System.Void",{});
        context_get=m.Resolve("","HPICNGFLLCA","NNEMNIPFKGA","RPG.GameCore.TaskContext",{});
        skill_ctor=m.Resolve("","OHHPOGMFPBF",".ctor","System.Void",{});
        list_get=m.Resolve("","HPICNGFLLCA","IJJNBFPJDDP","System.Collections.Generic.List<NAOKFLDEDBJ>",{});
        tick=m.Resolve("","HPICNGFLLCA","DEGNGOFGEJB","System.Void",{"System.Single"});
        carrier_ctor=m.Resolve("","FBCJBMKPDEH",".ctor","System.Void",{"RPG.GameCore.ProjectileData","HPICNGFLLCA",
            "RPG.GameCore.GameEntity","RPG.GameCore.GameEntity","NBHAKGBHPOE","OHHPOGMFPBF","LACOFPCJAFI","System.Boolean","RPG.GameCore.NewProjectileConfig"});
        carrier_dispose=m.Resolve("","NAOKFLDEDBJ","Dispose","System.Void",{});
        partial_cleanup=m.Resolve("","FBCJBMKPDEH","KDONLCELGKG","System.Void",{});
        previous_set=m.Resolve("","NAOKFLDEDBJ","set_PreviousProjectile","System.Void",{"NAOKFLDEDBJ"});
        wrapper=Field(services,"","NAOKFLDEDBJ","FCKCHPNEEKB","UnityEngine.GameObject");
        position=Field(services,"","NAOKFLDEDBJ","AKOIKFDLNID","UnityEngine.Vector3");
        velocity=Field(services,"","NAOKFLDEDBJ","CNCEDLKOHFL","UnityEngine.Vector3");
        phase=Field(services,"","NAOKFLDEDBJ","<State>k__BackingField","RPG.GameCore.ProjectileState");
        ordered=Field(services,"","NAOKFLDEDBJ","<EnsureCallbackOrder>k__BackingField","System.Boolean");
        last_position=Field(services,"RPG.GameCore","AbilityComponent","_LastFinishProjectilePos","UnityEngine.Vector3");
        last_velocity=Field(services,"RPG.GameCore","AbilityComponent","_LastFinishProjectileVelocity","UnityEngine.Vector3");
        owner_kind=Field(services,"RPG.GameCore","GameEntity","_EntityType","RPG.GameCore.EntityType");
        physics_methods=ResolveProjectilePhysics(m);
        detail::ProjectileWrapperCapture::Prepare(m);
    }
    void Disconnect(Flight& f) {
        if(f.subscription){services.lifecycle->CancelBeforeDispose(f.subscription);f.subscription=0;}
    }
    void Stop() {
        status.cancelled=true;
        for(auto& f:flights) {
            if(f->physics && !f->physics->Stop())Error(f->physics->Status().error);
            if(f->wrappers && !f->wrappers->Stop())Error("projectile wrapper stop incomplete");
        }
    }
    bool Drain() {
        Stop();
        bool all=true;
        // Stop native ticks before disposal; never pass through arrival/hit.
        if(ability_constructed && !dispose_attempted) {
            dispose_attempted=true;const auto r=native::CallVoid(ability_dispose,ability);
            disposed=r.returned;if(!disposed)Error("private projectile ability disposal failed");
        }
        if(ability_constructed && !disposed)all=false;
        for(auto& f:flights) {
            if(f->carrier && !f->listed && !f->cleanup_attempted) {
                f->cleanup_attempted=true;
                const auto r=native::CallVoid(f->constructed?carrier_dispose:partial_cleanup,f->carrier);
                if(!r.returned){Error("partial projectile cleanup failed");f->capture_failed=true;}
            }
            for(auto& effect:f->effects){effect->Cancel();effect->Service();if(!effect->Status().closed)all=false;}
            if(f->physics && !f->physics->Close())all=false;
            if(f->wrappers && !f->wrappers->Drain(!f->constructed))all=false;
            if(f->capture_failed)all=false;
            Disconnect(*f);
        }
        if(!all)return false;
        for(auto& f:flights)if(f->inputs && !f->inputs->Close())all=false;
        for(auto& f:flights)if(!f->Release())all=false;
        if(!all)return false;
        if(submissions)submissions->Stop();
        for(auto it=held.begin();it!=held.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=held.erase(it);}else ++it;
        }
        if(owner_subscription){services.lifecycle->CancelBeforeDispose(owner_subscription);owner_subscription=0;}
        status.closed=held.empty();status.prepared=false;
        if(status.closed){status.active=0;status.draining=0;}return status.closed;
    }
};
OwnedProjectileGroup::OwnedProjectileGroup(ProjectileGroupServices s,EntityOrigin owner)
    :state_(std::make_shared<State>(std::move(s),owner)) {
    const auto& v=state_->services;
    if(!v.methods || !v.metadata || !v.objects || !v.fields || !v.lifecycle || !v.effects || !v.owner_current || !owner.token.entity || !owner.token.generation || !owner.world)
        throw std::invalid_argument("projectile group requires owned lifetime and presentation services");
}
OwnedProjectileGroup::~OwnedProjectileGroup() {
    auto& s=*state_;if(s.submissions)s.submissions->Stop();
    if(s.owner_subscription)s.services.lifecycle->CancelBeforeDispose(s.owner_subscription);
    for(auto& f:s.flights)s.Disconnect(*f);
}
bool OwnedProjectileGroup::Prepare() {
    const auto s=state_;auto& out=s->status;if(s->attempted || out.closed)return out.prepared;s->attempted=true;
    try {
        if(!s->Current())throw std::runtime_error("projectile owner is not current");
        s->Keep(s->owner.token.entity);s->Keep(s->owner.world);
        for(auto* input:s->services.retained_inputs)if(input)s->Keep(input);
        s->Bind();
        if(Read<int>(s->owner.token.entity,s->owner_kind)!=6)
            throw std::runtime_error("private projectile owner must be a StoryCharacter");
        s->component=s->New("RPG.GameCore","AbilityComponent");Require(native::CallVoid(s->component_ctor,s->component),"private AbilityComponent constructor");
        Require(native::CallVoid(s->owner_ref,s->component,s->owner.token.entity),"private ability owner");
        auto* config=s->New("RPG.GameCore","AdventureAbilityConfig");Require(native::CallVoid(s->config_ctor,config),"private ability config constructor");
        for(const char* task:{"OnStart","OnAdd","OnRemove","OnAbort"})
            if(Read<void*>(config,Field(s->services,"RPG.GameCore","AdventureAbilityConfig",task,"RPG.GameCore.TaskConfig[]")))
                throw std::runtime_error("new ability configuration has tasks");
        s->ability=s->New("","AEEKMHCMBPL");Require(native::CallVoid(s->ability_ctor,s->ability,s->component,config),"private ability constructor");
        s->ability_constructed=true;
        auto* context=s->Object(s->context_get,s->ability);
        s->skill=s->New("","OHHPOGMFPBF");Require(native::CallVoid(s->skill_ctor,s->skill),"private projectile context");
        s->Reference(s->skill,"","OHHPOGMFPBF","CGDCONMACEN","RPG.GameCore.TaskContext",context);
        s->list=s->Object(s->list_get,s->ability);
        const auto list_type=s->services.metadata->ReadObjectType(s->list);
        MetadataResolver list_methods({list_type});
        s->list_add=list_methods.Resolve(list_type.name_space.c_str(),list_type.name.c_str(),"Add","System.Void",{"NAOKFLDEDBJ"});
        s->list_contains=list_methods.Resolve(list_type.name_space.c_str(),list_type.name.c_str(),"Contains","System.Boolean",{"NAOKFLDEDBJ"});
        std::weak_ptr<State> weak=s;
        s->owner_subscription=s->services.lifecycle->BeforeDispose(s->owner.token,[weak](EntityToken){if(auto held=weak.lock())held->Stop();});
        s->submissions=std::make_unique<EffectSubmissions>(*s->services.methods,s->services.references,s->services.fields,
            [weak](GameEntity* entity){auto held=weak.lock();return held && held->submitting && entity==held->owner.token.entity?held->owner:EntityOrigin{};},
            [weak](const EffectSubmission& value){
                auto held=weak.lock();if(!held || !held->submitting)return;
                auto& f=*held->submitting;
                try {
                    auto request=held->services.effects->CreateRequest("projectile");
                    f.effects.push_back(std::move(request));
                    if(!f.effects.back()->TrackSubmitted(value.manager,value.request))f.capture_failed=true;
                }catch(const std::exception& e){f.capture_failed=true;held->Error(e.what());}
            },[](const EffectSubmission&,bool){},s->services.trace);
        s->submissions->ActivateOnCurrentThread();out.prepared=true;return true;
    }catch(const std::exception& e){s->Error(e.what());s->Drain();return false;}
}
ProjectileId OwnedProjectileGroup::Fire(const ProjectileLaunch& launch) {
    const auto s=state_;if(!s->status.prepared || s->status.cancelled || s->busy)return 0;
    s->busy=true;struct Exit{State& s;~Exit(){if(s.submitting && s.submitting->wrappers)s.submitting->wrappers->End();s.submitting=nullptr;s.busy=false;}} exit{*s};
    State::Flight* f=nullptr;
    try {
        if(!s->Current() || !launch.target_current || !launch.target_current() || launch.target.world!=s->owner.world ||
            !launch.target.token.entity || !launch.target.token.generation || s->services.lifecycle->IsDisposed(launch.target.token))
            throw std::runtime_error("projectile participant is not current");
        auto item=std::make_unique<State::Flight>(s->services.references);item->id=++s->next_id;item->target=launch.target;item->current=launch.target_current;
        item->inputs=std::make_unique<ProjectileInputs>(*s->services.methods,*s->services.metadata,*s->services.objects,s->services.references);
        item->physics=std::make_unique<ProjectilePhysics>(s->physics_methods,s->services.references,s->arrays,*s->services.objects);
        item->wrappers=std::make_unique<detail::ProjectileWrapperCapture>(s->services.references,s->physics_methods.lifetime);
        f=item.get();s->flights.push_back(std::move(item));
        if(!f->inputs->Prepare(launch.parameters,launch.values))throw std::runtime_error(f->inputs->Status().error);
        const auto& input=f->inputs->Status();f->Keep(launch.target.token.entity);
        for(auto* identity:launch.retained_inputs)if(identity)f->Keep(identity);
        const auto carrier=s->services.objects->AllocateObject("","FBCJBMKPDEH");Require(carrier.call,"private projectile allocation");
        f->carrier=f->Keep(carrier.object);s->submitting=f;f->wrappers->Begin(f->carrier);
        const auto made=native::CallVoid(s->carrier_ctor,f->carrier,input.legacy?input.parameters:nullptr,s->ability,
            launch.target.token.entity,s->owner.token.entity,static_cast<void*>(nullptr),s->skill,static_cast<void*>(nullptr),launch.ordered,
            input.legacy?nullptr:input.parameters);
        f->wrappers->End();s->submitting=nullptr;f->constructed=made.returned;
        auto* wrapper=Read<void*>(f->carrier,s->wrapper);
        const auto& births=f->wrappers->Objects();
        if(!wrapper && births.size()==1)wrapper=births.front();
        const bool isolated=f->physics->Isolate(wrapper);
        if(!f->wrappers->Complete() || births.size()>1 || (wrapper && (births.empty() || births.front()!=wrapper)))
            f->capture_failed=true;
        Require(made,"private projectile constructor");
        if(!isolated)throw std::runtime_error(f->physics->Status().error);
        if(input.expects_effect && (!wrapper || f->effects.empty()))throw std::runtime_error("projectile FlyEffect has no owned creation receipt");
        if(f->capture_failed)throw std::runtime_error("projectile effect receipt retention incomplete");
        if(Read<bool>(f->carrier,s->ordered)) {
            // Match the official factory's last still-listed ORDERED flight.
            for(auto it=s->flights.rbegin();it!=s->flights.rend();++it)if(it->get()!=f && (*it)->listed && !(*it)->finished && Read<bool>((*it)->carrier,s->ordered)) {
                Require(native::CallVoid(s->previous_set,f->carrier,(*it)->carrier),"projectile predecessor");break;
            }
        }
        const auto appended=native::CallVoid(s->list_add,s->list,f->carrier);
        // A failed void call may already have appended. Observe membership
        // before deciding which owner is responsible for disposal.
        bool listed=false;const auto observed=native::Call(s->list_contains,&listed,s->list,f->carrier);
        f->listed=listed;
        if(!observed.returned)f->capture_failed=true;
        Require(observed,"projectile append membership");Require(appended,"private projectile list append");
        if(!listed)throw std::runtime_error("projectile list did not retain its new carrier");
        std::weak_ptr<State> weak=s;
        f->subscription=s->services.lifecycle->BeforeDispose(launch.target.token,[weak](EntityToken){if(auto held=weak.lock())held->Stop();});
        ++s->status.submitted;++s->status.active;return f->id;
    }catch(const std::exception& e){s->Error(e.what());s->Stop();return 0;}
}
void OwnedProjectileGroup::Cancel(){state_->status.cancelled=true;}
const ProjectileGroupStatus& OwnedProjectileGroup::Status()const{return state_->status;}
void OwnedProjectileGroup::Service(float dt) {
    const auto s=state_;if(s->busy || s->status.closed)return;
    s->busy=true;struct Exit{bool& busy;~Exit(){busy=false;}}exit{s->busy};
    try {
        if(!std::isfinite(dt) || dt<0)throw std::invalid_argument("invalid projectile frame duration");
        if(!s->status.prepared || !s->Current())s->status.cancelled=true;
        for(auto& f:s->flights)if(!f->finished && f->listed && (!f->current() || s->services.lifecycle->IsDisposed(f->target.token)))s->status.cancelled=true;
        if(s->status.cancelled){s->Drain();return;}
        Require(native::CallVoid(s->tick,s->ability,dt),"private projectile update");
        s->status.active=0;s->status.draining=0;
        for(auto it=s->flights.begin();it!=s->flights.end();) {
            auto& f=*it;
            if(f->listed && !f->finished) {
                bool remains=false;Require(native::Call(s->list_contains,&remains,s->list,f->carrier),"private projectile membership");
                const int state=Read<int>(f->carrier,s->phase);
                if(!f->arrived && state>=2) {
                    f->arrived=true;++s->status.arrived;
                    const auto p=Read<Position3>(f->carrier,s->position),v=Read<Position3>(f->carrier,s->velocity);
                    WritePrivate(s->component,s->last_position,p);WritePrivate(s->component,s->last_velocity,v);
                    const auto& input=f->inputs->Status();
                    if(s->services.arrival)s->services.arrival({f->id,f->target,p,v,input.hit_effect,input.align_hit,input.attach_hit});
                }
                if(!remains){f->finished=true;s->Disconnect(*f);}else ++s->status.active;
            }
            for(auto& effect:f->effects)effect->Service();
            if(f->finished) {
                bool closed=f->physics->Close();
                closed=f->wrappers->Drain(false) && closed;
                for(const auto& effect:f->effects)closed=closed && effect->Status().closed;
                if(closed)closed=f->inputs->Close();
                if(closed)closed=f->Release();
                if(closed){it=s->flights.erase(it);continue;}
                ++s->status.draining;
            }
            ++it;
        }
    }catch(const std::exception& e){s->Error(e.what());s->Stop();}
}
}
