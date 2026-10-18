#include "../../../src/presentation/events/ManagedConfigProjection.hpp"
#include "../../../src/presentation/events/OwnedAnimationTaskRules.hpp"
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <hksr/presentation/events/AnimationEventConstruction.hpp>
#include <hksr/entity/EntityLifecycle.hpp>
#include <Windows.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <map>
#include <thread>
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
namespace {
struct Obj {
    bool enabled=true;
    Obj* reference=nullptr;
    std::string type,name;
    std::map<std::string,Obj*> fields;
    std::vector<Obj*> items;
};
std::vector<std::unique_ptr<Obj>> arena;
std::map<int,void*> roots;
int serial=0,info,clones=0,writes=0;
bool fail_set=false;
bool fail_original=false;
bool fail_builder=false,build_tasks=false,nested_construction=false;
int construction_depth=0;
void* child_context=nullptr;
DWORD incoming_error=0;
int original_calls=0,caster_calls=0;
void *last_logic=nullptr,*last_context=nullptr,*last_registration=nullptr,*last_file=nullptr,*last_metadata=nullptr;
Obj* last_events=nullptr;
void Check(bool ok) {if(!ok)throw std::runtime_error("managed configuration projection test failed");}
Obj* New(const std::string& type) {auto p=std::make_unique<Obj>();p->type=type;auto* raw=p.get();arena.push_back(std::move(p));return raw;}
bool GameTask(const std::string& name) {
    return name=="TaskConfig" || name=="VCameraNoiseChange" || name=="HeadLookAt" ||
        name=="GlobalTimeSlow" || name=="RemoveGlobalTimeSlow" || name=="GlobalMainIntensityEffect";
}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* p,int,int,void*){roots[++serial]=p;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
Obj* __fastcall Clone(Obj* source,void*) {
    ++clones;Check(!roots.empty());auto* copy=New(source->type);*copy=*source;return copy;
}
Obj* __fastcall Find(Obj* type,Obj* name,int flags,void*) {
    Check(flags==54);auto* field=New("Field");field->name=name->name;field->fields["owner"]=type;return field;
}
Obj* __fastcall Get(Obj* field,Obj* source,void*) {return source->fields.at(field->name);}
void __fastcall Set(Obj* field,Obj* object,Obj* value,int,void*,void*,void*) {
    ++writes;
    if(fail_set){fail_set=false;RaiseException(0xE0760301,0,0,nullptr);}
    if(field->name=="TaskEnabled")object->enabled=value->enabled;
    else object->fields[field->name]=value;
}
int __fastcall Length(Obj* object,void*) {return static_cast<int>(object->items.size());}
int __fastcall TextLength(Obj* object,void*) {for(const auto& item:arena)if(item.get()==object)return static_cast<int>(object->name.size());return -1;}
unsigned short __fastcall TextChar(Obj* object,int index,void*) {return static_cast<unsigned short>(object->name.at(index));}
Obj* __fastcall At(Obj* object,int index,void*) {return object->items.at(index);}
void __fastcall Put(Obj* object,Obj* value,int index,void*) {++writes;object->items.at(index)=value;}
Obj* __fastcall CreateArray(Obj*,int count,void*) {auto* object=New("Task[]");object->items.resize(count);return object;}
GameEntity* __fastcall Caster(void* context,void*) {++caster_calls;return static_cast<GameEntity*>(context);}
__declspec(noinline) Obj* __fastcall Build(void*,Obj* configs,void*,void*) {
    if(fail_builder)RaiseException(0xE0760303,0,0,nullptr);
    auto* result=New("Executor[]");
    for(auto* config:configs->items)if(config && config->enabled)result->items.push_back(New("Executor"));
    SetLastError(257);return result;
}
__declspec(noinline) void __fastcall Construct(void*,Obj* events,void*,void*,void* logic,void* context,void* registration,void* file,void* method) {
    incoming_error=GetLastError();
    ++original_calls;last_events=events;last_logic=logic;last_context=context;last_registration=registration;last_file=file;last_metadata=method;
    if(fail_original)RaiseException(0xE0760302,0,0,nullptr);
    if(build_tasks)Build(child_context?child_context:context,events->items[0]->fields.at("EventList"),New("Sequence"),method);
    if(nested_construction && !construction_depth) {
        ++construction_depth;Construct(nullptr,events,nullptr,nullptr,logic,context,nullptr,nullptr,method);--construction_depth;
    }
    SetLastError(256);
}
unsigned long InvokeConstruction(Obj* events,void* context,void* file) {
    __try{Construct(nullptr,events,nullptr,nullptr,&info,context,&serial,file,&info);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DeclaredField Field(const char* owner,const char* name,const char* type) {
    return {"Fixture",owner,name,type,0,6,true};
}
struct Provider:MethodResolver,ObjectMetadata,ManagedObjects,TypeRelationships {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind) override {
        MetadataResolver r({
            {"System","Object",{{"MemberwiseClone","System.Object",{},false,M(Clone)}}},
            {"System","String",{{"get_Length","System.Int32",{},false,M(TextLength)},{"get_Chars","System.Char",{"System.Int32"},false,M(TextChar)}}},
            {"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"},false,M(Find)}}},
            {"System","Array",{{"CreateInstance","System.Array",{"System.Type","System.Int32"},true,M(CreateArray)},
                {"get_Length","System.Int32",{},false,M(Length)}, {"GetValue","System.Object",{"System.Int32"},false,M(At)},
                {"SetValue","System.Void",{"System.Object","System.Int32"},false,M(Put)}}},
            {"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}},
            {"","JKNLHJLPIOO",{{"KJCNEJALIJO","GELIALJDFBL[]",{"RPG.GameCore.TaskContext",
                "System.Collections.Generic.IEnumerable<RPG.GameCore.TaskConfig>","BNBOAMEBAOE"},true,M(Build)}}},
            {"","HHECHLOIEJO",{{"FGAHFIBMPCP","System.Void",{"RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]",
                "RPG.GameCore.AnimGroupEventConfig[]","RPG.GameCore.LogicAnimEventEntry[]","RPG.GameCore.TaskContext","LDMKDJJLOIL","System.String"},false,M(Construct)}}}});
        return r.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void* p) override {
        auto* o=static_cast<Obj*>(p);
        if(o->type=="ObfuscatedTask")return {"",o->type,{}};
        if(o->type=="AdventureGameplayLockWithDelay")return {"RPG.Client",o->type,{}};
        if(o->type=="Field")return {"Fixture","Field",{
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
        const bool game=GameTask(o->type) || o->type=="SingleAnimEventConfig" || o->type=="SingleAnimEventConfig[]" || o->type=="TaskConfig[]" || o->type=="TargetAlias";
        return {o->type=="Boolean"?"System":game?"RPG.GameCore":"Fixture",o->type,{}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* p) override {
        if(static_cast<Obj*>(p)->type=="TargetAlias")return {{"RPG.GameCore","TargetAlias","Alias","System.String",offsetof(Obj,reference),6,true}};
        Check(static_cast<Obj*>(p)->type=="Boolean");return {{"System","Boolean","m_value","System.Boolean",0,6,true}};
    }
    ObjectResult ManagedType(const char*,const char* name) override {auto* o=New("Type");o->name=name;return {{true,0},o};}
    ObjectResult NewStringUtf8(const char* text) override {auto* o=New("String");o->name=text;return {{true,0},o};}
    ObjectResult AllocateObject(const char* ns,const char* type) override {
        Check(std::string(ns)=="System" && std::string(type)=="Boolean");auto* o=New(type);o->enabled=false;return {{true,0},o};
    }
    bool TypeDerivesFrom(const char* ns,const char* name,const char*,const char* base) override {
        const auto full=std::string(ns)+"."+name;
        return std::string(base)=="TaskConfig" && ((std::string(ns)=="RPG.GameCore" && GameTask(name)) ||
            full==".ObfuscatedTask" || full=="RPG.Client.AdventureGameplayLockWithDelay");
    }
    std::vector<DeclaredField> Schema(const char*,const char* name) {
        if(std::string(name)=="SingleAnimEventConfig")return {{"RPG.GameCore",name,"EventList","RPG.GameCore.TaskConfig[]",0,6,true}};
        if(std::string(name)=="TargetEvaluator")return {};
        if(std::string(name)=="HeadLookAt")return {{"RPG.GameCore",name,"PerformerType","RPG.GameCore.TargetEvaluator",offsetof(Obj,reference),6,true},
            {"RPG.GameCore","TaskConfig","TaskEnabled","System.Boolean",0,6,true}};
        if(std::string(name)=="ObfuscatedTask" || std::string(name)=="AdventureGameplayLockWithDelay")
            return {{"RPG.GameCore","TaskConfig","TaskEnabled","System.Boolean",0,6,true}};
        Check(GameTask(name));return {{"RPG.GameCore","TaskConfig","TaskEnabled","System.Boolean",0,6,true}};
    }
};
ConfigObjectShape Shape(void*,const DeclaredType& type) {
    ConfigObjectShape out;
    if(type.name=="Event")out.edges.push_back(Field("Event","tasks","Fixture.Task[]"));
    else if(type.name=="Branch")out.edges.push_back(Field("Branch","cases","Fixture.Case[]"));
    else if(type.name=="Case")out.edges.push_back(Field("Case","tasks","Fixture.Task[]"));
    else if(type.name=="Blocked") {out.disable_task=true;out.enabled=Field("Task","TaskEnabled","System.Boolean");}
    else Check(type.name=="Keep");
    return out;
}
}
void TestManagedConfigProjection() {
    Provider provider;ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    ArrayMethods arrays{{},M(Length),M(At),M(Put),{}};
    auto* blocked=New("Blocked");auto* kept=New("Keep");
    auto* tasks=New("Task[]");tasks->items={blocked,nullptr,kept};
    auto* branch_case=New("Case");branch_case->fields["tasks"]=tasks;
    auto* cases=New("Case[]");cases->items={branch_case,branch_case};
    auto* branch=New("Branch");branch->fields["cases"]=cases;
    auto* outer=New("Task[]");outer->items={branch,kept};
    auto* event=New("Event");event->name="state/time metadata unchanged";event->fields["tasks"]=outer;
    auto* events=New("Event[]");events->items={event};
    ManagedConfigProjection project(provider,provider,provider,refs,arrays,Shape);
    const auto result=project.Apply({events,tasks,nullptr});
    auto* output=static_cast<Obj*>(result.roots[0]);auto* inner=static_cast<Obj*>(result.roots[1]);
    Check(result.copied==8 && result.disabled==1 && output!=events && inner!=tasks && result.roots[2]==nullptr);
    Check(output->items[0]->name==event->name && inner->items[0]!=blocked && !inner->items[0]->enabled);
    auto* copied_cases=output->items[0]->fields.at("tasks")->items[0]->fields.at("cases");
    Check(copied_cases->items[0]==copied_cases->items[1] && copied_cases->items[0]->fields.at("tasks")==inner);
    Check(inner->items[1]==nullptr && inner->items[2]==kept && blocked->enabled && tasks->items[0]==blocked);
    Check(project.Close() && roots.empty() && project.Close());
    // Unchanged graphs are read and retained, but allocate no private config.
    blocked->enabled=false;const auto before=clones;
    ManagedConfigProjection unchanged(provider,provider,provider,refs,arrays,Shape);
    Check(unchanged.Apply({events}).copied==0 && clones==before && unchanged.Close() && roots.empty());blocked->enabled=true;
    // A managed write exception prevents publication and permits root cleanup.
    ManagedConfigProjection failed(provider,provider,provider,refs,arrays,Shape);fail_set=true;bool threw=false;
    try{failed.Apply({events});}catch(const std::exception&){threw=true;}
    Check(threw && blocked->enabled && events->items[0]==event && failed.Close() && roots.empty());
    // Direct accidental source writes are rejected even on the low-level API.
    ManagedConfigProjection source_write(provider,provider,provider,refs,arrays,Shape);threw=false;const auto previous=writes;
    try{source_write.DisableTask(blocked);}catch(const std::exception&){threw=true;}
    Check(threw && writes==previous && source_write.Close());
    // The same managed adapter through a real native detour. All nine physical
    // arguments are forwarded, and original SEH is not swallowed/replaced.
    auto* camera=New("VCameraNoiseChange");auto* config_tasks=New("TaskConfig[]");config_tasks->items={camera};
    auto* single=New("SingleAnimEventConfig");single->fields["EventList"]=config_tasks;
    auto* event_array=New("SingleAnimEventConfig[]");event_array->items={single};
    int actor,foreign,file,world,owner_tag;auto* entity=reinterpret_cast<GameEntity*>(&actor);
    std::vector<std::string> log;
    std::vector<AnimationEventSource> observed,finished;
    std::vector<AnimationTaskConstruction> constructed;
    std::vector<std::uint64_t> task_sources;
    std::vector<bool> outcomes;
    std::function<void()> on_observe,on_tasks;
    // Register observation BEFORE the policy. It must still receive the final
    // inputs, not a copy based on client registration order.
    AnimationEventConstruction observer(provider,refs,
        [&](GameEntity* e){return e==entity?EntityOrigin{{e,1},reinterpret_cast<GameWorld*>(&world)}:EntityOrigin{};},
        [&](const auto& source){observed.push_back(source);if(on_observe)on_observe();SetLastError(999);},
        [&](const auto& source,const auto& tasks){task_sources.push_back(source.serial);constructed.push_back(tasks);if(on_tasks)on_tasks();SetLastError(998);},
        [&](const auto& source,bool returned){finished.push_back(source);outcomes.push_back(returned);SetLastError(997);},
        [&](const auto& text){log.push_back(text);});
    AnimationEventPolicy policy(provider,provider,provider,provider,[&](auto* ns,auto* name){return provider.Schema(ns,name);},refs,
        [&](GameEntity* e){return EntityCreationScope::ContainsOwned(e,&owner_tag);},
        [&](const auto& line){log.push_back(line);},DisabledOwnedAnimationTasks({},{}),{{"RPG.GameCore.HeadLookAt","PerformerType"}});
    EntityCreationScope creation(reinterpret_cast<GameWorld*>(&world),&owner_tag);
    EntityCreationScope::FactoryCompleted(reinterpret_cast<GameWorld*>(&world),entity);
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array && caster_calls==0);
    policy.ActivateOnCurrentThread();
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array);
    Check(last_events->items[0]->fields.at("EventList")->items[0]!=camera && !last_events->items[0]->fields.at("EventList")->items[0]->enabled);
    Check(camera->enabled && event_array->items[0]==single && roots.empty());
    Check(last_logic==&info && last_context==&actor && last_registration==&serial && last_file==&file && last_metadata==&info);
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events==event_array && roots.empty());
    const auto before_thread=caster_calls;
    std::thread other_thread([&]{Check(InvokeConstruction(event_array,&actor,&file)==0);});other_thread.join();
    Check(last_events==event_array && caster_calls==before_thread);
    fail_original=true;Check(InvokeConstruction(event_array,&actor,&file)==0xE0760302);fail_original=false;
    Check(roots.empty() && camera->enabled && log.back().find("original_returned=0")!=std::string::npos);
    fail_set=true;Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array && roots.empty());
    Check(log.back().find("projection failed; original inputs forwarded")!=std::string::npos);
    auto* obfuscated=New("ObfuscatedTask");auto* lock_task=New("AdventureGameplayLockWithDelay");
    auto* slow=New("GlobalTimeSlow");auto* slow_remove=New("RemoveGlobalTimeSlow");auto* intensity=New("GlobalMainIntensityEffect");
    config_tasks->items={camera,obfuscated,lock_task,slow,slow_remove,intensity};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array && roots.empty());
    const auto& projected_tasks=last_events->items[0]->fields.at("EventList")->items;
    Check(!projected_tasks[0]->enabled && projected_tasks[1]==obfuscated && !projected_tasks[2]->enabled);
    Check(!projected_tasks[3]->enabled && !projected_tasks[4]->enabled && !projected_tasks[5]->enabled);
    Check(camera->enabled && obfuscated->enabled && lock_task->enabled && slow->enabled && slow_remove->enabled && intensity->enabled);
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events==event_array && camera->enabled && lock_task->enabled);
    Check(slow->enabled && slow_remove->enabled && intensity->enabled);
    // Unsupported branch is diagnosed without rolling back the camera edit.
    auto* opaque=New("Opaque");config_tasks->items={opaque,camera};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array && roots.empty());
    Check(last_events->items[0]->fields.at("EventList")->items[0]==opaque &&
        !last_events->items[0]->fields.at("EventList")->items[1]->enabled && camera->enabled);
    Check(log.back().find("uninspected=1")!=std::string::npos);
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events==event_array && roots.empty());
    auto* look=New("HeadLookAt");auto* alias=New("TargetAlias");auto* alias_text=New("String");alias_text->name="AllEnemy";
    look->reference=alias;alias->reference=alias_text;config_tasks->items={look};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && !last_events->items[0]->fields.at("EventList")->items[0]->enabled);
    Check(look->enabled && look->reference==alias && roots.empty()); // only owned private copy changed
    alias_text->name="Caster";
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array && look->enabled);
    alias_text->name="AllEnemy";
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events==event_array && look->enabled);
    look->reference=nullptr;
    Check(InvokeConstruction(event_array,&actor,&file)==0 && !last_events->items[0]->fields.at("EventList")->items[0]->enabled && look->enabled);
    // A research client must neither block the Framework policy from binding
    // nor take its routing/stop ownership. Exercise two disjoint owners on the
    // actual detour, not just an isolated collection helper.
    int third;
    AnimationEventPolicy second(provider,provider,provider,provider,[&](auto* ns,auto* name){return provider.Schema(ns,name);},refs,
        [&](GameEntity* e){return e==reinterpret_cast<GameEntity*>(&foreign);},
        [&](const auto& line){log.push_back(line);},{"RPG.GameCore.VCameraNoiseChange"});
    second.ActivateOnCurrentThread();config_tasks->items={camera};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array && camera->enabled && roots.empty());
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events!=event_array && camera->enabled && roots.empty());
    Check(InvokeConstruction(event_array,&third,&file)==0 && last_events==event_array);
    second.Stop();
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events==event_array);
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array && roots.empty());
    // Intentional overlap is a composition, not a competing singleton. One
    // disables camera tasks, the other external HeadLookAt performers.
    look->reference=alias;alias_text->name="AllEnemy";config_tasks->items={camera,look};
    AnimationEventPolicy overlap(provider,provider,provider,provider,[&](auto* ns,auto* name){return provider.Schema(ns,name);},refs,
        [&](GameEntity* e){return e==entity;},[&](const auto& line){log.push_back(line);},{"RPG.GameCore.VCameraNoiseChange"});
    overlap.ActivateOnCurrentThread();
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events!=event_array && roots.empty());
    Check(!last_events->items[0]->fields.at("EventList")->items[0]->enabled &&
        !last_events->items[0]->fields.at("EventList")->items[1]->enabled && camera->enabled && look->enabled);
    observer.ActivateOnCurrentThread();build_tasks=true;
    // Both policies have run before either observation. The returned task array
    // is empty although the actual config array still has two disabled entries.
    SetLastError(123);Check(InvokeConstruction(event_array,&actor,&file)==0 && GetLastError()==256 && incoming_error==123);
    Check(observed.size()==1 && finished.size()==1 && constructed.size()==1 && outcomes.back());
    Check(observed.back().events==last_events && observed.back().events!=event_array && observed.back().serial==finished.back().serial);
    Check(task_sources.back()==observed.back().serial && constructed.back().parent && constructed.back().original_returned);
    Check(static_cast<Obj*>(constructed.back().configurations)->items.size()==2 && static_cast<Obj*>(constructed.back().tasks)->items.empty());
    Check(roots.empty());
    // Public entities and other threads stay transparent. Task work initiated
    // by an observer is not attributed to the interrupted game construction.
    const auto observations=observed.size();
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && observed.size()==observations);
    std::thread non_owner([&]{Check(InvokeConstruction(event_array,&actor,&file)==0);});non_owner.join();
    Check(observed.size()==observations);
    on_observe=[&]{Build(&actor,config_tasks,nullptr,nullptr);};
    on_tasks=[&]{Build(&actor,config_tasks,nullptr,nullptr);};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && constructed.size()==2 && roots.empty());
    on_observe={};on_tasks={};
    // Nested baseline construction is observed with null registration and file,
    // and has a distinct serial even when the context/config are reused.
    nested_construction=true;Check(InvokeConstruction(event_array,&actor,&file)==0);nested_construction=false;
    Check(observed.size()==4 && constructed.size()==4 && observed[2].serial!=observed[3].serial);
    Check(!observed[3].registration && !observed[3].file && task_sources[2]==observed[2].serial && task_sources[3]==observed[3].serial);
    Check(finished[2].serial==observed[3].serial && finished[3].serial==observed[2].serial && roots.empty());
    fail_builder=true;Check(InvokeConstruction(event_array,&actor,&file)==0xE0760303);fail_builder=false;
    Check(!outcomes.back() && !constructed.back().original_returned && !constructed.back().tasks && roots.empty());
    fail_original=true;Check(InvokeConstruction(event_array,&actor,&file)==0xE0760302);fail_original=false;
    Check(!outcomes.back() && roots.empty());
    child_context=&foreign;
    Check(InvokeConstruction(event_array,&actor,&file)==0 && constructed.back().context==&foreign && roots.empty());
    child_context=nullptr;
    on_observe=[] {throw std::runtime_error("observer fixture");};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && outcomes.back() && roots.empty());on_observe={};
    // Stopping from inside a callback suppresses subsequent notifications but
    // still releases the in-flight construction's roots.
    const auto finished_before_stop=finished.size();on_observe=[&]{observer.Stop();};
    Check(InvokeConstruction(event_array,&actor,&file)==0 && finished.size()==finished_before_stop && roots.empty());on_observe={};
    build_tasks=false;
    overlap.Stop();
    AnimationEventPolicy remaining(provider,provider,provider,provider,[&](auto* ns,auto* name){return provider.Schema(ns,name);},refs,
        [&](GameEntity* e){return e==reinterpret_cast<GameEntity*>(&foreign);},
        [&](const auto& line){log.push_back(line);},{"RPG.GameCore.VCameraNoiseChange"});
    remaining.ActivateOnCurrentThread();policy.Stop();
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array);
    Check(InvokeConstruction(event_array,&foreign,&file)==0 && last_events!=event_array && roots.empty());
    remaining.Stop();
    // Observation is independently useful without a policy and independently
    // stoppable when another observer has the same owner.
    int first_calls=0,second_calls=0;
    auto identity=[&](GameEntity* e){return e==entity?EntityOrigin{{e,2},reinterpret_cast<GameWorld*>(&world)}:EntityOrigin{};};
    AnimationEventConstruction first(provider,refs,identity,[&](const auto& s){Check(s.events==event_array);++first_calls;},
        [](const auto&,const auto&){},[](const auto&,bool){ });
    AnimationEventConstruction second_observer(provider,refs,identity,[&](const auto& s){Check(s.events==event_array);++second_calls;},
        [](const auto&,const auto&){},[](const auto&,bool){ });
    first.ActivateOnCurrentThread();second_observer.ActivateOnCurrentThread();
    Check(InvokeConstruction(event_array,&actor,nullptr)==0 && first_calls==1 && second_calls==1 && roots.empty());
    first.Stop();Check(InvokeConstruction(event_array,&actor,nullptr)==0 && first_calls==1 && second_calls==2 && roots.empty());
    second_observer.Stop();
    policy.Stop();const auto calls_before_stop=caster_calls;
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array && caster_calls==calls_before_stop);
    arena.clear();
    std::cout<<"Managed config projection passed: nested typed copies, source/owner/thread isolation, GC cleanup, real native detour argument forwarding and original SEH. Synthetic provider only.\n";
}
