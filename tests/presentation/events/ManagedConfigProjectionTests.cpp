#include "../../../src/presentation/events/ManagedConfigProjection.hpp"
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
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
int original_calls=0,caster_calls=0;
void *last_logic=nullptr,*last_context=nullptr,*last_registration=nullptr,*last_file=nullptr,*last_metadata=nullptr;
Obj* last_events=nullptr;
void Check(bool ok) {if(!ok)throw std::runtime_error("managed configuration projection test failed");}
Obj* New(const std::string& type) {auto p=std::make_unique<Obj>();p->type=type;auto* raw=p.get();arena.push_back(std::move(p));return raw;}
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
__declspec(noinline) void __fastcall Construct(void*,Obj* events,void*,void*,void* logic,void* context,void* registration,void* file,void* method) {
    ++original_calls;last_events=events;last_logic=logic;last_context=context;last_registration=registration;last_file=file;last_metadata=method;
    if(fail_original)RaiseException(0xE0760302,0,0,nullptr);
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
            {"","HHECHLOIEJO",{{"FGAHFIBMPCP","System.Void",{"RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]",
                "RPG.GameCore.AnimGroupEventConfig[]","RPG.GameCore.LogicAnimEventEntry[]","RPG.GameCore.TaskContext","LDMKDJJLOIL","System.String"},false,M(Construct)}}}});
        return r.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void* p) override {
        auto* o=static_cast<Obj*>(p);
        if(o->type=="Field")return {"Fixture","Field",{
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
        const bool game=o->type=="SingleAnimEventConfig" || o->type=="SingleAnimEventConfig[]" || o->type=="VCameraNoiseChange" || o->type=="TaskConfig[]" || o->type=="HeadLookAt" || o->type=="TargetAlias";
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
    bool TypeDerivesFrom(const char*,const char* name,const char*,const char* base) override {
        return std::string(base)=="TaskConfig" && (std::string(name)=="TaskConfig" || std::string(name)=="VCameraNoiseChange" || std::string(name)=="HeadLookAt");
    }
    std::vector<DeclaredField> Schema(const char*,const char* name) {
        if(std::string(name)=="SingleAnimEventConfig")return {{"RPG.GameCore",name,"EventList","RPG.GameCore.TaskConfig[]",0,6,true}};
        if(std::string(name)=="TargetEvaluator")return {};
        if(std::string(name)=="HeadLookAt")return {{"RPG.GameCore",name,"PerformerType","RPG.GameCore.TargetEvaluator",offsetof(Obj,reference),6,true},
            {"RPG.GameCore","TaskConfig","TaskEnabled","System.Boolean",0,6,true}};
        Check(std::string(name)=="VCameraNoiseChange");return {{"RPG.GameCore","TaskConfig","TaskEnabled","System.Boolean",0,6,true}};
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
    AnimationEventPolicy policy(provider,provider,provider,provider,[&](auto* ns,auto* name){return provider.Schema(ns,name);},refs,
        [&](GameEntity* e){return EntityCreationScope::ContainsOwned(e,&owner_tag);},
        [&](const auto& line){log.push_back(line);},{"RPG.GameCore.VCameraNoiseChange"},{{"RPG.GameCore.HeadLookAt","PerformerType"}});
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
    policy.Stop();const auto calls_before_stop=caster_calls;
    Check(InvokeConstruction(event_array,&actor,&file)==0 && last_events==event_array && caster_calls==calls_before_stop);
    arena.clear();
    std::cout<<"Managed config projection passed: nested typed copies, source/owner/thread isolation, GC cleanup, real native detour argument forwarding and original SEH. Synthetic provider only.\n";
}
