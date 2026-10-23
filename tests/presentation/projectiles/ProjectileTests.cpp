#include <hksr/presentation/projectiles/ProjectileInputs.hpp>
#include <hksr/presentation/projectiles/ProjectilePhysics.hpp>
#include <hksr/presentation/projectiles/OwnedProjectileGroup.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <Windows.h>
#include <iostream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool ok){if(!ok)throw std::runtime_error("projectile check failed");}
struct Object {
    std::string type,text;
    int behavior=1;
    bool trigger=true,ray=true,formation=true,align=true,attach=false,last=true,velocity=true;
    bool enabled=true,alive=true,active=true;
    Object *hit=nullptr,*unique=nullptr,*speed=nullptr,*angle=nullptr,*curve=nullptr,*expr=nullptr,*fly=nullptr;
    float number=0;
    std::vector<Object*> items;
    Object *owner=nullptr,*context=nullptr,*list=nullptr,*wrapper=nullptr,*previous=nullptr,*effect=nullptr;
    Position3 position,velocity3,last_position,last_velocity;
    int phase=0,index=0;unsigned effect_id=0;
    bool ordered=false,released=false;
};
std::vector<std::unique_ptr<Object>> arena;
std::map<int,void*> roots;
int serial=0,clone_count=0,scalar_conversions=0,collider_writes=0,body_writes=0;
bool fail_field=false,fail_collider=false;
bool fail_append=false,fail_constructor=false,fail_wrapper=false,delay_destroy=false;
bool nested_foreign=false;
unsigned disposals=0,partial_disposals=0;
Object* effect_manager=nullptr;
Object* foreign_wrapper=nullptr;
std::vector<Object*> carriers;
Object *collider_type=nullptr,*body_type=nullptr,*colliders=nullptr,*bodies=nullptr;
Object* New(const std::string& type){auto o=std::make_unique<Object>();o->type=type;auto* p=o.get();arena.push_back(std::move(o));return p;}
template<class F>Method M(F f){return {reinterpret_cast<void*>(f),nullptr};}
int __fastcall Keep(void* p,int,int kind,void*){Check(kind==2);roots[++serial]=p;return serial;}
void* __fastcall GetRoot(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
ReferenceMethods Refs(){return {M(Keep),M(GetRoot),M(Free),{}, {}};}
Object* __fastcall Clone(Object* p,void*){++clone_count;auto* c=New(p->type);*c=*p;return c;}
Object* __fastcall Find(Object*,Object* n,int flags,void*){Check(flags==54);auto* f=New("Field");f->text=n->text;return f;}
void __fastcall Set(Object* f,Object* p,Object* value,int,void*,void*,void*) {
    if(fail_field)RaiseException(0xe0830101,0,0,nullptr);
    if(f->text=="HitEffect")p->hit=value;
    else if(f->text=="FlyEffectUniqueName")p->unique=value;
    else if(f->text=="FlySpeed")p->speed=value;
    else if(f->text=="LinearPitchAngle")p->angle=value;
    else if(f->text=="CGDCONMACEN")p->context=value;
    else throw std::runtime_error("unexpected field write");
}
Object* __fastcall Number(float n,void*){++scalar_conversions;auto* p=New("DynamicFloat");p->number=n;return p;}
int __fastcall Length(Object* p,void*){return static_cast<int>(p->items.size());}
int __fastcall TextLength(Object* p,void*){return static_cast<int>(p->text.size());}
Object* __fastcall At(Object* p,int i,void*){return p->items.at(i);}
Object* __fastcall Components(Object*,Object* t,bool inactive,void*){Check(inactive);return t==collider_type?colliders:bodies;}
bool __fastcall Enabled(Object* p,void*){return p->enabled;}
void __fastcall ColliderSet(Object* p,bool enabled,void*){++collider_writes;if(fail_collider)RaiseException(0xe0830102,0,0,nullptr);p->enabled=enabled;}
void __fastcall BodySet(Object* p,bool enabled,void*){++body_writes;p->enabled=enabled;}
bool __fastcall Alive(Object* p,void*){return p->alive;}
void __fastcall Active(Object* p,bool value,void*){p->active=value;}
Method GroupResolve(const char*,const char*,const char*,const char*,std::initializer_list<const char*>,MethodKind);
DeclaredType GroupType(Object*);
std::vector<DeclaredField> GroupFields(const char*,const char*);
struct Provider:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind)override {
        MetadataResolver r({
            {"System","Object",{{"MemberwiseClone","System.Object",{},false,M(Clone)}}},
            {"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"},false,M(Find)}}},
            {"System","String",{{"get_Length","System.Int32",{},false,M(TextLength)}}},
            {"RPG.GameCore","DynamicFloat",{{"BIIMDAFMJNF","RPG.GameCore.DynamicFloat",{"System.Single"},true,M(Number)}}}});
        if(std::string(type)=="Object" && std::string(name)=="MemberwiseClone")return r.Resolve(ns,type,name,result,args,kind);
        if(std::string(type)=="RuntimeType" || std::string(type)=="DynamicFloat" || std::string(type)=="String")return r.Resolve(ns,type,name,result,args,kind);
        return GroupResolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void* value)override {
        auto* p=static_cast<Object*>(value);
        if(p->type=="Field")return {"Fixture","Field",{{"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
        if(p->type=="List")return GroupType(p);
        return {"RPG.GameCore",p->type,{}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* value)override {
        const auto* p=static_cast<Object*>(value);
        if(p->type=="DynamicFloat")return {{"RPG.GameCore",p->type,"postfixExpr","JJAABNBJMEH",offsetof(Object,expr),6,true}};
        const auto field=[&](const char* n,const char* t,std::size_t o){return DeclaredField{"RPG.GameCore",p->type,n,t,o,6,true};};
        return {field("Behavior","RPG.GameCore.ProjectileMoveBehavior",offsetof(Object,behavior)),
            field("TriggerHitCallback","System.Boolean",offsetof(Object,trigger)),
            field("EnableRayCast","System.Boolean",offsetof(Object,ray)),
            field("CorrectedWhenTargetFormationChange","System.Boolean",offsetof(Object,formation)),
            field("AlignHitEffect","System.Boolean",offsetof(Object,align)),
            field("AttachHitEffectToTargetEntity","System.Boolean",offsetof(Object,attach)),
            field("StartFromLastHitPos","System.Boolean",offsetof(Object,last)),
            field("UseLastHitVelocity","System.Boolean",offsetof(Object,velocity)),
            field("HitEffect","System.String",offsetof(Object,hit)),
            field("FlyEffect","System.String",offsetof(Object,fly)),
            field("FlyEffectUniqueName","System.String",offsetof(Object,unique)),
            field("FlySpeed","RPG.GameCore.DynamicFloat",offsetof(Object,speed)),
            field("LinearPitchAngle","RPG.GameCore.DynamicFloat",offsetof(Object,angle)),
            field("CustomSpeedEaseCurve","System.String",offsetof(Object,curve))};
    }
    ObjectResult ManagedType(const char*,const char* name)override {
        if(std::string(name)=="Collider")return {{true,0},collider_type};
        if(std::string(name)=="Rigidbody")return {{true,0},body_type};
        auto* p=New("Type");p->text=name;return {{true,0},p};
    }
    ObjectResult NewStringUtf8(const char* name)override{auto* p=New("String");p->text=name;return {{true,0},p};}
    ObjectResult AllocateObject(const char*,const char* name)override{return {{true,0},New(name)};}
};
void Inputs() {
    Provider p;auto* source=New("ProjectileData");source->hit=New("String");source->unique=New("String");
    source->curve=New("String");source->speed=New("DynamicFloat");source->angle=New("DynamicFloat");
    for(int family:{1,2,4,7}) {
        source->behavior=family;ProjectileInputs request(p,p,p,Refs());Check(request.Prepare(source));
        const auto status=request.Status();auto* c=static_cast<Object*>(status.parameters);
        Check(c!=source && !c->trigger && !c->ray && !c->formation && !c->hit && !c->unique);
        Check(c->curve==source->curve && c->speed==source->speed && status.hit_effect==source->hit);
        Check(status.start_from_last_hit && status.use_last_velocity && status.align_hit);
        Check(source->trigger && source->ray && source->formation && source->hit && source->unique);
        Check(request.Close() && roots.empty());
    }
    for(const char* type:{"LinearProjectileData","RandomTraceProjectileData"}) {
        source->type=type;ProjectileInputs request(p,p,p,Refs());Check(request.Prepare(source));
        Check(!request.Status().legacy && request.Close() && roots.empty());
    }
    source->type="ProjectileData";source->behavior=1;source->speed->expr=New("Expression");
    {ProjectileInputs request(p,p,p,Refs());const int before=clone_count;Check(!request.Prepare(source));
     Check(request.Status().unresolved_values==std::vector<std::string>{"FlySpeed"});Check(clone_count==before && roots.empty());}
    {ProjectileInputs request(p,p,p,Refs());Check(request.Prepare(source,{{"FlySpeed",3.5f}}));
     auto* c=static_cast<Object*>(request.Status().parameters);Check(c->speed!=source->speed && c->speed->number==3.5f && !c->speed->expr);
     Check(source->speed->expr && request.Close() && roots.empty());}
    source->speed->expr=nullptr;source->angle->expr=New("Expression");source->behavior=2;
    {ProjectileInputs request(p,p,p,Refs());Check(request.Prepare(source));Check(request.Close() && roots.empty());} // unused linear parameter
    source->angle->expr=nullptr;
    {ProjectileInputs request(p,p,p,Refs());Check(!request.Prepare(source,{{"FlySpeed",std::numeric_limits<float>::infinity()}}));Check(roots.empty());}
    {ProjectileInputs request(p,p,p,Refs());Check(!request.Prepare(source,{{"Typo",1.0f}}));Check(roots.empty());}
    source->behavior=3;
    {ProjectileInputs request(p,p,p,Refs());Check(!request.Prepare(source));Check(roots.empty());}
    source->behavior=1;fail_field=true;
    {ProjectileInputs request(p,p,p,Refs());Check(!request.Prepare(source));Check(source->hit && source->trigger && roots.empty());}
    fail_field=false;
}
void Physics() {
    Provider provider;collider_type=New("ColliderType");body_type=New("BodyType");
    colliders=New("Array");bodies=New("Array");
    auto* c1=New("Collider");auto* c2=New("Collider");auto* b=New("Rigidbody");auto* untouched=New("OtherCollider");
    colliders->items={c1,nullptr,c2};bodies->items={b};
    ProjectilePhysicsMethods m{M(Components),M(Enabled),M(ColliderSet),M(Enabled),M(BodySet),M(Active),{M(Alive),{}}};
    ManagedArrays arrays({{},M(Length),M(At),{}, {}});
    auto* wrapper=New("GameObject");
    {ProjectilePhysics request(m,Refs(),arrays,provider);Check(request.Isolate(wrapper));
     Check(!c1->enabled && !c2->enabled && !b->enabled && untouched->enabled);
     Check(request.Status().colliders==2 && request.Status().rigidbodies==1 && wrapper->active);
     const int writes=collider_writes;Check(request.Isolate(wrapper) && writes==collider_writes);
     Check(!request.Close() && !roots.empty());Check(request.Stop() && !wrapper->active);
     Check(!request.Close());wrapper->alive=false;Check(request.Close() && roots.empty());}
    wrapper=New("GameObject");c1->enabled=true;c2->enabled=true;b->enabled=true;fail_collider=true;
    {ProjectilePhysics request(m,Refs(),arrays,provider);const int writes=collider_writes;Check(!request.Isolate(wrapper));
     Check(!wrapper->active && !b->enabled && collider_writes==writes+2 && !request.Status().error.empty());
     Check(!request.Close());wrapper->alive=false;Check(request.Close() && roots.empty());}
    fail_collider=false;
    {ProjectilePhysics request(m,Refs(),arrays,provider);Check(request.Isolate(nullptr) && request.Close() && roots.empty());}
}
void __fastcall EmptyCtor(Object*,void*){}
void __fastcall OwnerRef(Object* p,Object* owner,void*){p->owner=owner;}
void __fastcall AbilityCtor(Object* p,Object* component,Object* config,void*) {
    Check(component->type=="AbilityComponent" && config->type=="AdventureAbilityConfig");
    p->owner=component;p->context=New("TaskContext");p->context->owner=component->owner;p->list=New("List");
}
Object* __fastcall ContextGet(Object* p,void*){return p->context;}
void __fastcall SkillCtor(Object* p,void*){p->index=-1;}
Object* __fastcall ListGet(Object* p,void*){return p->list;}
void __fastcall Append(Object* p,Object* value,void*){p->items.push_back(value);if(fail_append)RaiseException(0xe0830103,0,0,nullptr);}
bool __fastcall Contains(Object* p,Object* value,void*){return std::find(p->items.begin(),p->items.end(),value)!=p->items.end();}
void __fastcall Previous(Object* p,Object* value,void*){p->previous=value;}
unsigned __fastcall EffectId(Object* p,void*){return p->effect_id;}
bool __fastcall Released(Object* p,void*){return p->released;}
int __fastcall EffectState(Object*,void*){return 0;}
void* __fastcall EffectInstance(Object*,void*){return nullptr;}
bool __fastcall EffectRemove(void*,EffectReference r,bool,void*){static_cast<Object*>(r.parameters)->released=true;return true;}
__declspec(noinline) Object* __fastcall SyncEffect(void* manager,Object* p,void*) {
    Check(manager==effect_manager && p->owner);p->number+=1;return p;
}
__declspec(noinline) void __fastcall AsyncEffect(void* manager,Object* p,void*){SyncEffect(manager,p,nullptr);}
__declspec(noinline) void __fastcall Trigger(Object* p,void*){p->number+=1;}
Object* __fastcall Caster(Object* p,void*){return p->owner;}
__declspec(noinline) void __fastcall UnityCtor(Object* p,Object* name,void*) {
    p->text=name?name->text:"projectile";p->active=true;p->alive=true;p->number+=1;
}
Method UnityCtorThunk() {
    // The production binding must follow the same import slot that DPM uses,
    // not attach its observer to a bypassed managed .ctor wrapper.
    struct Thunk {unsigned char bytes[16]{};void* target=nullptr;};
    static Thunk t;const unsigned char code[]{0x48,0x8b,0x05,9,0,0,0,0xff,0xe0};
    std::memcpy(t.bytes,code,sizeof(code));t.target=reinterpret_cast<void*>(&UnityCtor);
    return {t.bytes,nullptr};
}
__declspec(noinline) Object* __fastcall WrapperFactory(Object* carrier,Object* effect,void*) {
    Check(carrier && effect);
    if(nested_foreign){nested_foreign=false;foreign_wrapper=WrapperFactory(New("UnownedCarrier"),effect,nullptr);}
    auto* wrapper=New("Wrapper");UnityCtor(wrapper,effect,nullptr);
    if(fail_wrapper)RaiseException(0xe0830105,0,0,nullptr);
    return wrapper;
}
void __fastcall CarrierCtor(Object* p,Object* legacy,Object* ability,Object* target,Object* caster,void* hit,
    Object* skill,void* finish,bool ordered,Object* modern,void*) {
    Check(!hit && !finish && target && caster==ability->owner->owner && skill->context==ability->context && skill->index==-1);
    auto* config=legacy?legacy:modern;Check(config && !config->trigger && !config->ray && !config->hit);
    p->owner=ability;p->ordered=ordered && config->behavior!=2;
    p->position={1,2,3};p->velocity3={4,5,6};p->effect=New("Request");p->effect->owner=caster;p->effect->effect_id=unsigned(carriers.size()+1);
    SyncEffect(effect_manager,p->effect,nullptr);carriers.push_back(p);
    p->wrapper=WrapperFactory(p,p->effect,nullptr);
    if(fail_constructor)RaiseException(0xe0830104,0,0,nullptr);
}
void __fastcall CarrierDispose(Object* p,void*) {
    ++disposals;if(p->effect)p->effect->released=true;
    if(p->wrapper && !delay_destroy)p->wrapper->alive=false;
}
void __fastcall PartialCleanup(Object* p,void*){++partial_disposals;CarrierDispose(p,nullptr);}
void __fastcall AbilityDispose(Object* p,void*){for(auto* f:p->list->items)CarrierDispose(f,nullptr);p->list->items.clear();}
void __fastcall Tick(Object* p,float dt,void*) {
    for(auto it=p->list->items.begin();it!=p->list->items.end();) {
        auto* f=*it;
        if(f->phase==3){f->number-=dt;if(f->number<=0){CarrierDispose(f,nullptr);it=p->list->items.erase(it);continue;}}
        else if(f->phase==2){f->phase=3;f->number=0.5f;}
        else {f->number+=dt;if(f->number>=1)f->phase=2;}
        ++it;
    }
}
void __fastcall Destroy(Object* p,void*){p->alive=false;}
DeclaredType GroupType(Object*) {return {"Fixture","List",{
    {"Add","System.Void",{"NAOKFLDEDBJ"},false,M(Append)},
    {"Contains","System.Boolean",{"NAOKFLDEDBJ"},false,M(Contains)}}};}
std::vector<DeclaredField> GroupFields(const char* ns,const char* type) {
    const auto f=[&](const char* n,const char* t,std::size_t o){return DeclaredField{ns,type,n,t,o,6,true};};
    if(std::string(type)=="AdventureAbilityConfig")return {f("OnStart","RPG.GameCore.TaskConfig[]",offsetof(Object,hit)),
        f("OnAdd","RPG.GameCore.TaskConfig[]",offsetof(Object,unique)),f("OnRemove","RPG.GameCore.TaskConfig[]",offsetof(Object,curve)),
        f("OnAbort","RPG.GameCore.TaskConfig[]",offsetof(Object,expr))};
    if(std::string(type)=="AbilityComponent")return {f("_LastFinishProjectilePos","UnityEngine.Vector3",offsetof(Object,last_position)),
        f("_LastFinishProjectileVelocity","UnityEngine.Vector3",offsetof(Object,last_velocity))};
    if(std::string(type)=="NAOKFLDEDBJ")return {f("FCKCHPNEEKB","UnityEngine.GameObject",offsetof(Object,wrapper)),
        f("AKOIKFDLNID","UnityEngine.Vector3",offsetof(Object,position)),f("CNCEDLKOHFL","UnityEngine.Vector3",offsetof(Object,velocity3)),
        f("<State>k__BackingField","RPG.GameCore.ProjectileState",offsetof(Object,phase)),
        f("<EnsureCallbackOrder>k__BackingField","System.Boolean",offsetof(Object,ordered))};
    if(std::string(type)=="OHHPOGMFPBF")return {f("CGDCONMACEN","RPG.GameCore.TaskContext",offsetof(Object,context))};
    if(std::string(type)=="GameEntity")return {f("_EntityType","RPG.GameCore.EntityType",offsetof(Object,behavior))};
    if(std::string(type)=="TriggerEffectParams")return {f("CasterEntity","RPG.GameCore.GameEntity",offsetof(Object,owner)),
        f("<UniqueEffectID>k__BackingField","System.UInt32",offsetof(Object,effect_id))};
    if(std::string(type)=="NFGENDDOCJJ")return {f("OFKGLJOAMLD","RPG.GameCore.TriggerEffect",offsetof(Object,hit)),
        f("DDDNLILNMKA","RPG.GameCore.TaskContext",offsetof(Object,context))};
    throw std::runtime_error("unexpected group fields");
}
Method GroupResolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind) {
    MetadataResolver r({
        {"RPG.GameCore","AbilityComponent",{{".ctor","System.Void",{},false,M(EmptyCtor)}}},
        {"RPG.GameCore","GameComponentBase",{{"InitOwnerRef","System.Void",{"RPG.GameCore.GameEntity"},false,M(OwnerRef)}}},
        {"RPG.GameCore","AdventureAbilityConfig",{{".ctor","System.Void",{},false,M(EmptyCtor)}}},
        {"","AEEKMHCMBPL",{{".ctor","System.Void",{"RPG.GameCore.AbilityComponent","RPG.GameCore.AdventureAbilityConfig"},false,M(AbilityCtor)},
            {"Dispose","System.Void",{},false,M(AbilityDispose)}}},
        {"","HPICNGFLLCA",{{"NNEMNIPFKGA","RPG.GameCore.TaskContext",{},false,M(ContextGet)},
            {"IJJNBFPJDDP","System.Collections.Generic.List<NAOKFLDEDBJ>",{},false,M(ListGet)},
            {"DEGNGOFGEJB","System.Void",{"System.Single"},false,M(Tick)}}},
        {"","OHHPOGMFPBF",{{".ctor","System.Void",{},false,M(SkillCtor)}}},
        {"","FBCJBMKPDEH",{{".ctor","System.Void",{"RPG.GameCore.ProjectileData","HPICNGFLLCA","RPG.GameCore.GameEntity","RPG.GameCore.GameEntity",
            "NBHAKGBHPOE","OHHPOGMFPBF","LACOFPCJAFI","System.Boolean","RPG.GameCore.NewProjectileConfig"},false,M(CarrierCtor)},
            {"KDONLCELGKG","System.Void",{},false,M(PartialCleanup)}}},
        {"","NAOKFLDEDBJ",{{"Dispose","System.Void",{},false,M(CarrierDispose)},
            {"DPMCKGFPGKO","UnityEngine.GameObject",{"RPG.Client.MonoEffect"},false,M(WrapperFactory)},
            {"set_PreviousProjectile","System.Void",{"NAOKFLDEDBJ"},false,M(Previous)}}},
        {"UnityEngine","GameObject",{{"GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"},false,M(Components)},
            {".ctor","System.Void",{"System.String"},false,UnityCtorThunk()},
            {"SetActive","System.Void",{"System.Boolean"},false,M(Active)}}},
        {"UnityEngine","Collider",{{"get_enabled","System.Boolean",{},false,M(Enabled)},{"set_enabled","System.Void",{"System.Boolean"},false,M(ColliderSet)}}},
        {"UnityEngine","Rigidbody",{{"get_detectCollisions","System.Boolean",{},false,M(Enabled)},{"set_detectCollisions","System.Void",{"System.Boolean"},false,M(BodySet)}}},
        {"UnityEngine","Object",{{"IsNativeObjectAlive","System.Boolean",{"UnityEngine.Object"},true,M(Alive)},{"Destroy","System.Void",{"UnityEngine.Object"},true,M(Destroy)}}},
        {"RPG.Client","MonoEffectManager",{{"FireEffectAsync","System.Void",{"RPG.Client.TriggerEffectParams"},false,M(AsyncEffect)},
            {"FireEffect","RPG.Client.MonoEffect",{"RPG.Client.TriggerEffectParams"},false,M(SyncEffect)}}},
        {"","NFGENDDOCJJ",{{"OnTaskBegin","System.Void",{},false,M(Trigger)}}},
        {"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}}});
    return r.Resolve(ns,type,name,result,args,kind);
}
void Groups() {
    Provider provider;EntityLifecycle lifecycle;auto* actor=New("Actor");actor->behavior=6;auto* target=New("Actor");auto* world=New("World");
    const EntityOrigin owner{lifecycle.Track(reinterpret_cast<GameEntity*>(actor)),reinterpret_cast<GameWorld*>(world)};
    const EntityOrigin other{lifecycle.Track(reinterpret_cast<GameEntity*>(target)),owner.world};
    effect_manager=New("Manager");auto* parameters=New("ProjectileData");parameters->hit=New("String");
    parameters->fly=New("String");parameters->fly->text="Effects/Projectile";
    bool current=true;unsigned arrivals=0;
    EffectAccessMethods access;access.id=M(EffectId);access.released=M(Released);access.state=M(EffectState);
    access.instance=M(EffectInstance);access.remove=M(EffectRemove);
    EffectRequestServices effects{Refs(),EffectAccess(access),{},[]{return ObjectResult{{true,0},effect_manager};},{}};
    ProjectileGroupServices services;services.methods=&provider;services.metadata=&provider;services.objects=&provider;services.fields=GroupFields;
    services.references=Refs();services.arrays={{},M(Length),M(At),{}, {}};services.lifecycle=&lifecycle;
    services.effects=std::make_shared<PresentationEffects>(effects);services.owner_current=[&]{return current;};
    services.arrival=[&](const ProjectileArrival& a){++arrivals;Check(a.position.x==1 && a.velocity.z==6 && a.hit_effect==parameters->hit);};
    ProjectileLaunch launch;launch.parameters=parameters;launch.target=other;launch.target_current=[&]{return current;};
    colliders->items.clear();bodies->items.clear();
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());const auto base_roots=roots.size();const auto start=carriers.size();
     nested_foreign=true;Check(g.Fire(launch)==1 && g.Fire(launch)==2);Check(carriers[start+1]->previous==carriers[start]);
     // Submissions from the same caster OUTSIDE this constructor are not owned.
     auto* unrelated=New("Request");unrelated->owner=actor;unrelated->effect_id=999;SyncEffect(effect_manager,unrelated,nullptr);
     g.Service(1);Check(arrivals==2 && g.Status().active==2);
     Check(carriers[start]->owner->owner->last_position.x==1); // private AbilityComponent only
     Check(actor->last_position.x==0 && target->last_position.x==0);
     g.Service(0);g.Service(0.6f);Check(g.Status().active==0 && g.Status().draining==0 && roots.size()==base_roots);
     Check(g.Fire(launch)==3);g.Service(1);g.Service(0);g.Service(0.6f);Check(roots.size()==base_roots);
     g.Cancel();g.Service(0);Check(g.Status().closed && !unrelated->released && roots.empty());
     Check(foreign_wrapper && foreign_wrapper->alive && foreign_wrapper->active);}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());Check(g.Fire(launch));const auto before=disposals;
     delay_destroy=true;g.Cancel();g.Service(0);Check(!g.Status().closed && disposals==before+1 && !carriers.back()->wrapper->active);
     g.Service(0);Check(disposals==before+1);carriers.back()->wrapper->alive=false;delay_destroy=false;g.Service(0);Check(g.Status().closed && roots.empty());}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());fail_append=true;const auto before=disposals;
     Check(!g.Fire(launch));fail_append=false;g.Service(0);Check(g.Status().closed && disposals==before+1 && roots.empty());}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());fail_constructor=true;const auto before=partial_disposals;
     Check(!g.Fire(launch));fail_constructor=false;g.Service(0);Check(g.Status().closed && partial_disposals==before+1 && roots.empty());}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());fail_wrapper=true;const auto before=partial_disposals;
     Check(!g.Fire(launch));fail_wrapper=false;Check(!carriers.back()->wrapper);
     g.Service(0);g.Service(0);Check(g.Status().closed && partial_disposals==before+1 && roots.empty());}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());Check(g.Fire(launch));
     current=false;g.Service(0);Check(g.Status().closed && roots.empty());current=true;}
    {actor->behavior=3;OwnedProjectileGroup g(services,owner);Check(!g.Prepare());
     Check(g.Status().closed && roots.empty());actor->behavior=6;}
    {OwnedProjectileGroup g(services,owner);Check(g.Prepare());Check(g.Fire(launch));
     lifecycle.DisposalStarting(other.token);Check(!carriers.back()->wrapper->active);g.Service(0);Check(g.Status().closed && roots.empty());}
    Check(actor->active && actor->last_position.x==0 && target->last_position.x==0);
}
}
int main(){try{Inputs();Physics();Groups();std::cout<<"Projectile inputs, physics and lifecycle passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
