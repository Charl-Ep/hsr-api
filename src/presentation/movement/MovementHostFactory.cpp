#include "MovementHostFactory.hpp"
#include <hksr/managed/GenericFactory.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <optional>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
namespace {
template<class T>T Read(void* object,std::size_t offset) {
    T value{};
    if(!object || !native::CopyMemoryChecked(&value,static_cast<char*>(object)+offset,sizeof(value)))
        throw std::runtime_error("movement host field unreadable");
    return value;
}
std::size_t Field(RuntimeMetadata& m,const char* space,const char* type,const char* name,const char* value_type) {
    std::optional<std::size_t> out;
    for(const auto& f:m.ReadFields(space,type))if(f.name==name) {
        if(out || f.type!=value_type || !f.flags_observed || (f.flags&0x10))
            throw std::runtime_error(std::string("movement field differs: ")+type+"."+name);
        out=f.offset;
    }
    if(!out)throw std::runtime_error(std::string("movement field absent: ")+type+"."+name);
    return *out;
}
}
struct MovementHostFactory::State {
    RuntimeMetadata& metadata;ReferenceMethods references;ManagedRoots roots;GenericFactory factory;
    Method component,destroy,initialize,root_get,root_set,callback_get,callback_set,animator_get,is_instance;
    std::size_t external_root=0,external_move=0,proxy=0,thread_data=0,pending=0;
    void* movement_type=nullptr;void* adventure_type=nullptr;void* transform_type=nullptr;void* data_type=nullptr;
    void* provider_type=nullptr;
    std::function<void(const std::string&)> trace;
    AnimationMovementAccessBinding access;
    std::vector<ManagedRootId> handles;
    struct Entry {EntityToken owner;std::shared_ptr<OwnedMovementHost> lease;};
    std::vector<Entry> active;
    bool closed=false;
    State(RuntimeMetadata& m,ReferenceMethods r,ManagedArrays a):metadata(m),references(r),roots(r),
        factory(ResolveGenericFactoryMethods(m),r,m,std::move(a)){}
    void* Type(const char* ns,const char* name) {
        const auto value=metadata.ManagedType(ns,name);
        if(!value.call.returned || !value.object)throw std::runtime_error(std::string("movement Type unavailable: ")+name);
        handles.reserve(handles.size()+1);handles.push_back(roots.Keep(value.object));
        if(!roots.Status(handles.back()).ready)throw std::runtime_error("movement Type retention failed");
        return value.object;
    }
    ObjectResult Component(GameEntity* entity,void* type)const {
        ObjectResult out;out.call=native::Call(component,&out.object,entity,type);return out;
    }
    void Prepare() {
        auto& m=metadata;
        component=m.Resolve("RPG.GameCore","GameEntity","GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"});
        destroy=m.Resolve("RPG.GameCore","GameEntity","DestroyComponent","System.Void",{"System.Type"});
        initialize=m.Resolve("","BGOBCKLPEMK","LMCEDCHJKFF","System.Void",
            {"IBEJMEEEJMB","RPG.GameCore.TransformComponent","RPG.GameCore.CharacterDataComponent","ODAKFODAONI"});
        root_get=m.Resolve("RPG.GameCore","AdventureCharacterController","get_EnableRootMotion","System.Boolean",{});
        root_set=m.Resolve("RPG.GameCore","AdventureCharacterController","set_EnableRootMotion","System.Void",{"System.Boolean"});
        callback_get=m.Resolve("","BGOBCKLPEMK","get_EnableAnimatorMoveCallback","System.Boolean",{});
        callback_set=m.Resolve("","BGOBCKLPEMK","set_EnableAnimatorMoveCallback","System.Void",{"System.Boolean"});
        animator_get=m.Resolve("RPG.GameCore","AdventureCharacterController","GetUnityAnimator","UnityEngine.Animator",{});
        is_instance=m.Resolve("System","Type","IsInstanceOfType","System.Boolean",{"System.Object"});
        external_root=Field(m,"RPG.GameCore","AdventureCharacterController","EnableRootMotionExternal","System.Func<System.Boolean>");
        external_move=Field(m,"RPG.GameCore","AdventureCharacterController","<ExternalMoveHandler>k__BackingField","MHHGLBGPCFH");
        proxy=Field(m,"RPG.GameCore","AdventureCharacterController","_MovementProxy","KMEOMKNPNCC");
        thread_data=Field(m,"","KMEOMKNPNCC","<ThreadData>k__BackingField","FEIBCGELFAJ");
        pending=Field(m,"","FEIBCGELFAJ","LCEIFCIKKAN","System.Boolean");
        access=ResolveAnimationMovementAccess460(m);
        // The unique declared zero-argument factory is verified before name-
        // based reflection. Never select a runtime code address by RVA here.
        unsigned matched=0;
        for(const auto& method:m.ReadType("RPG.GameCore","GameEntity").methods)
            if(method.name=="CreateComponent") {
                if(!method.parameters.empty() || method.is_static || method.result!="T")throw std::runtime_error("CreateComponent declaration changed");
                ++matched;
            }
        if(matched!=1)throw std::runtime_error("CreateComponent declaration is not unique");
        movement_type=Type("","BGOBCKLPEMK");adventure_type=Type("RPG.GameCore","AdventureCharacterController");
        transform_type=Type("RPG.GameCore","TransformComponent");data_type=Type("RPG.GameCore","CharacterDataComponent");
        provider_type=Type("","IBEJMEEEJMB");
        auto* entity_type=Type("RPG.GameCore","GameEntity");
        if(!factory.Prepare(entity_type,"CreateComponent",movement_type))throw std::runtime_error(factory.Error());
    }
};
MovementHostFactory::MovementHostFactory(RuntimeMetadata& m,ReferenceMethods r,ManagedArrays a,std::function<void(const std::string&)> trace)
    :state_(std::make_shared<State>(m,r,std::move(a))) {
    state_->trace=std::move(trace);
    try{state_->Prepare();}catch(...){Close();throw;}
}
MovementHostFactory::~MovementHostFactory()=default;
std::shared_ptr<OwnedMovementHost> MovementHostFactory::Acquire(EntityToken token,void* animator,
    std::function<bool()> current,std::function<bool()> alive) {
    const auto s=state_;
    if(s->closed || !current || !alive || !current() || !alive())throw std::runtime_error("movement owner unavailable");
    Collect();
    for(const auto& e:s->active)if(e.owner.entity==token.entity && e.owner.generation==token.generation && !e.lease->Status().closed)
        throw std::runtime_error("owned entity already has a movement lease");
    const auto adventure=s->Component(token.entity,s->adventure_type),transform=s->Component(token.entity,s->transform_type),
        data=s->Component(token.entity,s->data_type),existing=s->Component(token.entity,s->movement_type);
    if(!adventure.call.returned || !transform.call.returned || !data.call.returned || !existing.call.returned || !transform.object)
        throw std::runtime_error("movement host prerequisites unreadable");
    if(!adventure.object && !existing.object)throw std::runtime_error("new movement host requires its Adventure animator provider");
    if(adventure.object) {
        bool provider=false;
        if(!native::Call(s->is_instance,&provider,s->provider_type,adventure.object).returned || !provider)
            throw std::runtime_error("Adventure does not implement the native movement Animator provider");
        if(Read<void*>(adventure.object,s->external_root) || Read<void*>(adventure.object,s->external_move))
            throw std::runtime_error("Adventure movement is owned by an external controller");
        void* actual=nullptr;
        if(!native::Call(s->animator_get,&actual,adventure.object).returned || actual!=animator)
            throw std::runtime_error("Adventure animator differs from the requested model");
    }
    MovementHostServices services;services.references=s->references;
    services.current=[s,adventure,current=std::move(current)] {
        return current() && (!adventure.object || (!Read<void*>(adventure.object,s->external_root) && !Read<void*>(adventure.object,s->external_move)));
    };
    services.alive=std::move(alive);
    services.trace=s->trace;
    services.retained_inputs={adventure.object,transform.object,data.object,animator};
    services.lookup=[s,token]{return s->Component(token.entity,s->movement_type);};
    services.create=[s,token]{return s->factory.Invoke(token.entity);};
    services.initialize=[s,adventure,transform,data](void* c){return native::CallVoid(s->initialize,c,adventure.object,transform.object,data.object,static_cast<void*>(nullptr));};
    services.remove=[s,token](void* c){
        const auto found=s->Component(token.entity,s->movement_type);
        if(!found.call.returned || found.object!=c)return CallResult{};
        return native::CallVoid(s->destroy,token.entity,s->movement_type);
    };
    services.observe=[s,token](void* c){return AnimationMovementAccess(s->access,token.entity).Observe(c);};
    services.read_root_motion=[s,adventure]{Observed<bool> out{false,{true,0}};
        if(adventure.object)out.read=native::Call(s->root_get,&out.value,adventure.object);return out;};
    services.write_root_motion=[s,adventure](bool value){return adventure.object?native::CallVoid(s->root_set,adventure.object,value):CallResult{true,0};};
    services.read_pending_motion=[s,adventure]{Observed<bool> out{false,{true,0}};
        if(adventure.object){auto* proxy=Read<void*>(adventure.object,s->proxy);auto* data=Read<void*>(proxy,s->thread_data);
            out.value=Read<bool>(data,s->pending);}return out;};
    services.read_callback=[s](void* c){Observed<bool> out;out.read=native::Call(s->callback_get,&out.value,c);return out;};
    services.write_callback=[s](void* c,bool value){return native::CallVoid(s->callback_set,c,value);};
    auto lease=std::make_shared<OwnedMovementHost>(std::move(services),token);
    s->active.push_back({token,lease}); // keep failed cleanup receipts, too
    lease->Prepare();return lease;
}
void MovementHostFactory::Collect() {
    auto& active=state_->active;
    for(auto it=active.begin();it!=active.end();) {
        if(it->lease.use_count()==1 || it->lease->Status().closed)it->lease->Close();
        if(it->lease->Status().closed)it=active.erase(it);else ++it;
    }
}
bool MovementHostFactory::Close() {
    auto& s=*state_;s.closed=true;Collect();
    if(!s.active.empty())return false;
    const bool factory=s.factory.Close();
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.handles.erase(it);}else ++it;
    }
    return factory && s.handles.empty();
}
}
