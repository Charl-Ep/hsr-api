#include "../../../src/presentation/timeline/TimelineRegistryAccess.hpp"
#include "../../../src/presentation/timeline/TimelineRegistryHooks.hpp"
#include "../../../src/presentation/timeline/TimelineRegistryTracking.hpp"
#include "../../../src/presentation/timeline/TimelineTasks.hpp"
#include "../../../src/presentation/timeline/TimelineWaits.hpp"
#include "../../../src/presentation/timeline/CommonTimelinePlayback.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include "../../../src/presentation/PresentationClients.hpp"
#include <Windows.h>
#include <cstddef>
#include <iostream>
#include <map>
#include <algorithm>
#include <thread>

using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
namespace {
void CheckAt(bool ok,int line){if(!ok)throw std::runtime_error("Timeline registry boundary test failed line "+std::to_string(line));}
#define Check(...) CheckAt((__VA_ARGS__),__LINE__)
enum Tag { RegistryTag,RecordTag,ListTag,DictionaryTag,StringTag,DirectorTag,FieldTag,TypeTag,HistoryTag };
struct Text {Tag tag=StringTag;std::wstring value;};
struct Record {Tag tag=RecordTag;void* director=nullptr;Text* resource=nullptr;Text* name=nullptr;int id=0,phase=0;};
struct List {Tag tag=ListTag;std::vector<Record*> items;};
struct Dictionary {Tag tag=DictionaryTag;std::map<void*,Record*> items;};
struct History {Tag tag=HistoryTag;std::vector<std::pair<Text*,int>> items;};
History official_history;
struct Registry {Tag tag=RegistryTag;List* list=nullptr;Dictionary* dictionary=nullptr;History* history=&official_history;};
Registry* timeline_manager=nullptr;
Tag manager_field=FieldTag;
bool fail_after_play=false;
unsigned __fastcall RuntimeId(void*,void*){return 123;}
bool __fastcall EntityLoaded(void*,void*){return true;}
void* __fastcall ManagerValue(void* field,void* target,void*){Check(field==&manager_field && !target);return timeline_manager;}
std::vector<std::unique_ptr<Registry>> registry_views;
std::vector<std::unique_ptr<History>> private_histories;
std::vector<std::unique_ptr<List>> private_lists;
int history_frame=100;
bool fail_history_write=false,fail_record_stop=false;
struct Entity {void* world=nullptr;};
struct Context {Entity* caster=nullptr;};
struct Task {int padding=17;Context* context=nullptr;Record* saved=nullptr;int state=0;unsigned camera=0;};
Task* running_task=nullptr;
Tag reflected_field=FieldTag,reflected_type=TypeTag;
Tag history_reflected_field=FieldTag,query_reflected_field=FieldTag;
bool fail_list_read=false,fail_query_write=false;int fail_list_item=-1;
bool fail_caster=false;int task_index=-1;float task_delta=0;
std::function<void(int)> on_task;
Entity* __fastcall Caster(Context* c,void*) {
    if(fail_caster)RaiseException(0xe0760420,0,0,nullptr);
    return c->caster;
}
void* __fastcall World(Entity* e,void*){return e->world;}
void* __fastcall GetField(void* type,Text* name,int flags,void*) {
    if(name->value==L"s_TimelineManager"){Check(flags==56);return &manager_field;}
    Check(type==&reflected_type && flags==54);
    if(name->value==L"DFNAOFNEBHC")return &history_reflected_field;
    if(name->value==L"MKDJOLKJJON")return &query_reflected_field;
    Check(name->value==L"BMHGECJDFGN");return &reflected_field;
}
void __fastcall SetField(void* field,void* target,void* value,int flags,void* binder,void* culture,void*) {
    Check(flags==0 && !binder && !culture);
    if(field==&query_reflected_field) {
        if(fail_query_write)RaiseException(0xe0760426,0,0,nullptr);
        Check(value && *static_cast<Tag*>(value)==ListTag);
        static_cast<Registry*>(target)->list=static_cast<List*>(value);return;
    }
    if(field==&history_reflected_field) {
        Check(value!=nullptr);
        if(fail_history_write)RaiseException(0xe0760424,0,0,nullptr);
        static_cast<Registry*>(target)->history=static_cast<History*>(value);return;
    }
    Check(field==&reflected_field && !value);static_cast<Task*>(target)->saved=nullptr;
}
void* __fastcall CloneRegistry(Registry* source,void*) {
    Check(source->tag==RegistryTag);registry_views.push_back(std::make_unique<Registry>(*source));return registry_views.back().get();
}
int metadata=0,generic_metadata=1;
void* __fastcall EmptyHistory(History* source,int index,int count,void* info) {
    Check(source->tag==HistoryTag && index==0 && count==0 && info==&generic_metadata);
    private_histories.push_back(std::make_unique<History>());return private_histories.back().get();
}
std::map<int,void*> handles;std::map<int,int> handle_kinds;int sequence=0,borrow_sequence=0;
int fail_at=0,free_calls=0;bool fail_lookup=false,fail_free=false;
void* seen_metadata=nullptr;void* seen_asset=nullptr;const void* seen_position=nullptr;const void* seen_rotation=nullptr;
std::function<void()> on_borrow,on_return;
template<class T>Method M(T f,void* info=&metadata){return {reinterpret_cast<void*>(f),info};}
int __fastcall Keep(void* p,int,int kind,void*){handles[++sequence]=p;handle_kinds[sequence]=kind;return sequence;}
void* __fastcall Target(int id,void*){return handles.at(id);}
void __fastcall Free(int id,void*){++free_calls;if(fail_free)RaiseException(0xe0760411,0,0,nullptr);handles.erase(id);handle_kinds.erase(id);}
void CollectFixtureObject(void* object) {
    for(const auto& h:handles)Check(h.second!=object || handle_kinds.at(h.first)==0);
    for(auto& h:handles)if(h.second==object)h.second=nullptr;
}
int __fastcall Length(Text* s,void*){return static_cast<int>(s->value.size());}
wchar_t __fastcall Character(Text* s,int i,void*){return s->value.at(i);}
int __fastcall Count(List* l,void*){if(fail_list_read)RaiseException(0xe0760427,0,0,nullptr);return static_cast<int>(l->items.size());}
void* __fastcall Item(List* l,int i,void*){if(i==fail_list_item)RaiseException(0xe0760428,0,0,nullptr);return l->items.at(i);}
void* __fastcall CopyList(List* l,int index,int count,void* info) {
    Check(info==&generic_metadata && index==0 && count==static_cast<int>(l->items.size()));
    private_lists.push_back(std::make_unique<List>(*l));return private_lists.back().get();
}
void __fastcall RemoveListItem(List* l,int index,void*){l->items.erase(l->items.begin()+index);}
std::map<void*,Text*> director_assets;
std::map<void*,int> director_states;
void* __fastcall PlayableAsset(void* director,void*){return director_assets.at(director);}
void* __fastcall ObjectName(void* asset,void*){return asset;}
int __fastcall DirectorState(void* director,void*){return director_states.at(director);}
bool __fastcall StringEqual(Text* a,Text* b,void*){return a==b || (a && b && a->value==b->value);}
bool __fastcall Find(Dictionary* d,void* director,void** out,void* info) {
    Check(info==&generic_metadata);
    if(fail_lookup)RaiseException(0xe0760401,0,0,nullptr);
    const auto i=d->items.find(director);*out=i==d->items.end()?nullptr:i->second;return i!=d->items.end();
}
// These functions are synthetic native targets, not substitutes claiming to
// emulate all game branches. Mutations/order are the audited boundary contract.
__declspec(noinline) void* __fastcall Borrow(Registry* r,Text* name,Text* resource,void* asset,const void* pos,const void* rot,void* info) {
    seen_metadata=info;seen_asset=asset;seen_position=pos;seen_rotation=rot;
    auto* record=r->list->items.at(0);record->name=name;record->resource=resource;
    if(fail_at==1)RaiseException(0xe0760402,0,0,nullptr); // before a new ID
    record->phase=1;record->id=++borrow_sequence;
    if(on_borrow)on_borrow();
    if(fail_at==2)RaiseException(0xe0760403,0,0,nullptr); // after a new ID
    return record->director;
}
__declspec(noinline) void __fastcall Return(Registry* r,void* director,void* info) {
    seen_metadata=info;auto* record=r->dictionary->items.at(director);
    if(record->name && !record->name->value.empty()) {
        auto& history=r->history->items;
        if(history.size()>1)history.erase(history.begin());
        history.emplace_back(record->name,history_frame);
    }
    if(on_return)on_return(); // the old state and ownership are still visible
    if(fail_at==3)RaiseException(0xe0760404,0,0,nullptr);
    record->phase=0;
}
__declspec(noinline) void __fastcall Remove(Registry* r,Record* record,void* info) {
    seen_metadata=info;
    if(fail_at==4)RaiseException(0xe0760405,0,0,nullptr);
    r->dictionary->items.erase(record->director);
    r->list->items.erase(std::find(r->list->items.begin(),r->list->items.end(),record));
}
__declspec(noinline) void __fastcall Dispose(Registry* r,void* info) {
    seen_metadata=info;
    if(fail_at==5)RaiseException(0xe0760406,0,0,nullptr);
    r->list->items.clear();r->dictionary->items.clear();
}
int stop_calls=0,lookup_calls=0;
int playing_calls=0;
__declspec(noinline) void __fastcall StopRecord(Record* record,void* info) {
    seen_metadata=info;
    if(fail_record_stop)RaiseException(0xe0760423,0,0,nullptr);
    if(record->phase==2 || record->phase==3)record->phase=4;
}
__declspec(noinline) bool __fastcall Recent(Registry* r,Text* name,int frames,void* info) {
    seen_metadata=info;
    for(auto i=r->history->items.rbegin();i!=r->history->items.rend();++i) {
        if(i->second<history_frame-frames)return false;
        if(!name)RaiseException(0xe0760425,0,0,nullptr);
        if(i->first->value.find(name->value)!=std::wstring::npos)return true;
    }
    return false;
}
__declspec(noinline) bool __fastcall AssetPlaying(Registry* r,Text* name,void* info) {
    ++playing_calls;seen_metadata=info;
    for(auto* row:r->list->items)if(StringEqual(director_assets.at(row->director),name,nullptr))
        return director_states.at(row->director)==1;
    return false;
}
__declspec(noinline) void __fastcall StopName(Registry* r,Text* name,void* info) {
    ++stop_calls;seen_metadata=info;
    for(auto* row:r->list->items)if(row->name && name && row->name->value==name->value && row->phase>=1 && row->phase<=3) {
        if(fail_record_stop)RaiseException(0xe0760423,0,0,nullptr);
        if(row->phase==2 || row->phase==3)row->phase=4;
        break;
    }
}
__declspec(noinline) void* __fastcall Resource(Registry* r,Text* path,void* info) {
    ++lookup_calls;seen_metadata=info;Record* result=nullptr;
    // 15dd361d: a null query is not an equality lookup for null resources.
    // The synthetic exception value only checks propagation, not Unity's code.
    if(!path) {
        for(auto* row:r->list->items)if(row)RaiseException(0xe0760422,0,0,nullptr);
        return nullptr;
    }
    for(auto* row:r->list->items)if(row && row->resource && row->resource->value==path->value) {
        if(row->phase==2)return row;
        if(row->phase==3 || row->phase==4)result=row;
    }
    return result;
}
DWORD InvokeResource(Registry* registry,Text* path,void** result) {
    __try{*result=Resource(registry,path,&metadata);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD InvokeRecent(Registry* registry,Text* name,int frames,bool* result) {
    __try{*result=Recent(registry,name,frames,&metadata);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD InvokeBorrow(Registry* r,Text* name,Text* resource,void* asset,const void* pos,const void* rot,void** result) {
    __try{*result=Borrow(r,name,resource,asset,pos,rot,&metadata);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
int __fastcall PlayCommon(Registry* registry,unsigned id,Text* path,void* asset,const float* position,const float* rotation,
    void* bind,void* complete,double time,int wrap,void* info) {
    Check(id==123 && !asset && !bind && !complete && time==0 && wrap==2 && info==&metadata);
    Check(position[0]==0 && position[1]==0 && position[2]==0 && rotation[0]==0 && rotation[1]==0 && rotation[2]==0);
    auto* director=Borrow(registry,path,path,asset,position,rotation,info);
    auto* record=registry->dictionary->items.at(director);record->phase=2;
    if(fail_after_play)RaiseException(0xe0760429,0,0,nullptr);
    return record->id;
}
DWORD InvokeEnd(TimelineBoundary operation,Registry* r,void* arg) {
    __try {
        if(operation==TimelineBoundary::Return)Return(r,arg,&metadata);
        if(operation==TimelineBoundary::Remove)Remove(r,static_cast<Record*>(arg),&metadata);
        if(operation==TimelineBoundary::Dispose)Dispose(r,&metadata);
        if(operation==TimelineBoundary::StopName)StopName(r,static_cast<Text*>(arg),&metadata);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<int Index>__declspec(noinline) void __fastcall TaskVoid(Task* task,void* info) {
    task_index=Index;seen_metadata=info;Check(task!=nullptr);running_task=task;
    if(on_task)on_task(Index);
}
template<int Index>__declspec(noinline) void __fastcall TaskTick(Task* task,float delta,void* info) {
    task_delta=delta;TaskVoid<Index>(task,info);
}
DWORD InvokeTask(Task* task) {
    __try{TaskVoid<0>(task,&metadata);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Provider:MethodResolver,ObjectMetadata,ManagedObjects {
    bool wrong_type=false,static_field=false;
    std::vector<std::unique_ptr<Text>> allocated;
    ObjectResult NewStringUtf8(const char* text) override {
        auto p=std::make_unique<Text>();
        const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);
        Check(count>0);p->value.resize(count);
        Check(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,p->value.data(),count)==count);p->value.pop_back();
        auto* raw=p.get();allocated.push_back(std::move(p));return {{true,0},raw};
    }
    ObjectResult AllocateObject(const char*,const char*) override {throw std::logic_error("not used");}
    ObjectResult ManagedType(const char* ns,const char* type) override {
        if(std::string(ns)=="RPG.Client" && std::string(type)=="GlobalVars")return {{true,0},&reflected_type};
        Check(std::string(ns).empty() && (std::string(type)=="JODBIGDDMOB" || std::string(type)=="IGBEIECLHMC" || std::string(type)=="HPPNAAGGAMP"));return {{true,0},&reflected_type};
    }
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind) override {
        MetadataResolver resolver({
            {"System","Object",{{"MemberwiseClone","System.Object",{},false,M(CloneRegistry)}}},
            {"","EBMMGODFEEO",{{"HFKFNFKIEON","System.Void",{},false,M(StopRecord)}}},
            {"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"},false,M(GetField)}}},
            {"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}},
            {"RPG.GameCore","GameEntity",{{"get_OwnerWorldRef","RPG.GameCore.GameWorld",{},false,M(World)},
                {"get_RuntimeID","System.UInt32",{},false,M(RuntimeId)},{"get_IsLoaded","System.Boolean",{},false,M(EntityLoaded)}}},
            {"","NCEDGGKOMIH",{{"NOLKCBFDAOJ","System.Int32",{"System.UInt32","System.String","UnityEngine.Playables.PlayableAsset",
                "UnityEngine.Vector3","UnityEngine.Vector3","System.Action<UnityEngine.Playables.PlayableDirector,UnityEngine.Playables.PlayableAsset>",
                "UnityEngine.Events.UnityAction","System.Double","UnityEngine.Playables.DirectorWrapMode"},false,M(PlayCommon)}}},
            {"","OKKFJLFMPFK",{
                {"OnTaskBegin","System.Void",{},false,M(TaskVoid<0>)},
                {"Tick","System.Void",{"System.Single"},false,M(TaskTick<11>)},
                {"OnTaskReset","System.Void",{},false,M(TaskVoid<12>)},
                {"Dispose","System.Void",{},false,M(TaskVoid<13>)},
                {"OnSkip","System.Void",{},false,M(TaskVoid<14>)}}},
            {"","MLAPLADNCKE",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<1>)}}},
            {"","JODBIGDDMOB",{
                {"OnTaskBegin","System.Void",{},false,M(TaskVoid<2>)},
                {"Tick","System.Void",{"System.Single"},false,M(TaskTick<3>)},
                {"OnTaskReset","System.Void",{},false,M(TaskVoid<4>)},
                {"Dispose","System.Void",{},false,M(TaskVoid<5>)}}},
            {"","IGBEIECLHMC",{
                {"OnTaskBegin","System.Void",{},false,M(TaskVoid<6>)},
                {"Tick","System.Void",{"System.Single"},false,M(TaskTick<7>)},
                {"OnTaskReset","System.Void",{},false,M(TaskVoid<8>)},
                {"Dispose","System.Void",{},false,M(TaskVoid<9>)}}},
            {"","INFHMIEOCGH",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<10>)}}},
            {"","MPKLOFIMGGL",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<15>)}}},
            {"","BKIDMGGCGKL",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<16>)}}},
            {"","OCBDDGMOFNF",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<17>)}}},
            {"","PIJKEKGCHIF",{
                {"OnTaskBegin","System.Void",{},false,M(TaskVoid<18>)},
                {"Tick","System.Void",{"System.Single"},false,M(TaskTick<19>)},
                {"OnTaskReset","System.Void",{},false,M(TaskVoid<20>)},
                {"Dispose","System.Void",{},false,M(TaskVoid<21>)}}},
            {"","CHGJJPLLGPM",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<22>)}}},
            {"","DMOBAEFEAEO",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<23>)}}},
            {"","AOHNNKDMBJA",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<24>)}}},
            {"","GLDOCBPPCFG",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<25>)}}},
            {"","BCPLNBHIENH",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<26>)}}},
            {"","IOKBPCJKEOL",{{"OnTaskBegin","System.Void",{},false,M(TaskVoid<27>)}}},
            {"","JCMHNJJLLNE",{
                {"OnTaskBegin","System.Void",{},false,M(TaskVoid<28>)},
                {"Tick","System.Void",{"System.Single"},false,M(TaskTick<29>)},
                {"OnTaskReset","System.Void",{},false,M(TaskVoid<30>)},
                {"Dispose","System.Void",{},false,M(TaskVoid<31>)}}},
            {"UnityEngine.Playables","PlayableDirector",{
                {"get_playableAsset","UnityEngine.Playables.PlayableAsset",{},false,M(PlayableAsset)},
                {"get_state","UnityEngine.Playables.PlayState",{},false,M(DirectorState)}}},
            {"UnityEngine","Object",{{"get_name","System.String",{},false,M(ObjectName)}}},
            {"System","String",{{"get_Length","System.Int32",{},false,M(Length)},{"get_Chars","System.Char",{"System.Int32"},false,M(Character)},
                {"op_Equality","System.Boolean",{"System.String","System.String"},true,M(StringEqual)}}},
            {"","HPPNAAGGAMP",{
                {"APGJAAKNKKI","UnityEngine.Playables.PlayableDirector",{"System.String","System.String","UnityEngine.Playables.PlayableAsset","UnityEngine.Vector3","UnityEngine.Vector3"},false,M(Borrow)},
                {"OFHBGOMIELI","System.Void",{"UnityEngine.Playables.PlayableDirector"},false,M(Return)},
                {"NKDINGGCODD","System.Void",{"EBMMGODFEEO"},false,M(Remove)},
                {"Dispose","System.Void",{},false,M(Dispose)},
                {"ADJGFHMINEF","System.Void",{"System.String"},false,M(StopName)},
                {"IODNDPEJOFF","EBMMGODFEEO",{"System.String"},false,M(Resource)},
                {"MOOCHPDGDFK","System.Boolean",{"System.String"},false,M(AssetPlaying)},
                {"EKDLILNHAAN","System.Boolean",{"System.String","System.Int32"},false,M(Recent)}}}});
        return resolver.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void* p) override {
        Check(p!=nullptr);
        switch(*static_cast<Tag*>(p)) {
        case RegistryTag:return {"","HPPNAAGGAMP",{}};
        case HistoryTag:return {"Fixture","HistoryList",{{"GetRange","System.Collections.Generic.List<System.Collections.Generic.KeyValuePair<System.String,System.Int32>>",
            {"System.Int32","System.Int32"},false,M(EmptyHistory,&generic_metadata)}}};
        case RecordTag:return {"","EBMMGODFEEO",{}};
        case ListTag:return {"Fixture","RecordList",{
            {"get_Count","System.Int32",{},false,M(Count)},{"get_Item","EBMMGODFEEO",{"System.Int32"},false,M(Item)},
            {"GetRange","System.Collections.Generic.List<EBMMGODFEEO>",{"System.Int32","System.Int32"},false,M(CopyList,&generic_metadata)},
            {"RemoveAt","System.Void",{"System.Int32"},false,M(RemoveListItem)}}};
        case DictionaryTag:return {"Fixture","DirectorDictionary",{
            {"TryGetValue","System.Boolean",{"UnityEngine.Playables.PlayableDirector","EBMMGODFEEO&"},false,M(Find,&generic_metadata)}}};
        case FieldTag:return {"Fixture","Field",{{"GetValue","System.Object",{"System.Object"},false,M(ManagerValue)},{"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(SetField)}}};
        default:throw std::runtime_error("unexpected synthetic type query");
        }
    }
    std::vector<DeclaredField> ReadObjectFields(void*) override {throw std::logic_error("not used");}
    std::vector<DeclaredField> Fields(const char*,const char* type) {
        const std::string task_type=type;
        if(task_type=="OKKFJLFMPFK" || task_type=="JODBIGDDMOB" || task_type=="IGBEIECLHMC" ||
            task_type=="PIJKEKGCHIF" || task_type=="JCMHNJJLLNE")
            return {{"",type,"DDDNLILNMKA","RPG.GameCore.TaskContext",offsetof(Task,context),1,true},
                {"",type,"BMHGECJDFGN","EBMMGODFEEO",offsetof(Task,saved),1,true}};
        if(task_type=="MLAPLADNCKE" || task_type=="INFHMIEOCGH" || task_type=="MPKLOFIMGGL" ||
            task_type=="BKIDMGGCGKL" || task_type=="OCBDDGMOFNF" || task_type=="CHGJJPLLGPM" ||
            task_type=="DMOBAEFEAEO" || task_type=="AOHNNKDMBJA" || task_type=="GLDOCBPPCFG" ||
            task_type=="BCPLNBHIENH" || task_type=="IOKBPCJKEOL")
            return {{"RPG.GameCore","ImmediateTaskBase`1","_TaskContext","RPG.GameCore.TaskContext",offsetof(Task,context),36,true}};
        if(std::string(type)=="HPPNAAGGAMP")return {
            {"",type,"DFNAOFNEBHC","System.Collections.Generic.List<System.Collections.Generic.KeyValuePair<System.String,System.Int32>>",offsetof(Registry,history),1,true},
            {"",type,"MKDJOLKJJON","System.Collections.Generic.List<EBMMGODFEEO>",offsetof(Registry,list),1,true},
            {"",type,"PIJENJPJFLL","System.Collections.Generic.Dictionary<UnityEngine.Playables.PlayableDirector,EBMMGODFEEO>",offsetof(Registry,dictionary),1,true}};
        return {{"",type,"LDALFGDPLLF","UnityEngine.Playables.PlayableDirector",offsetof(Record,director),6,true},
            {"",type,"DKMKBPLKNDC","System.String",offsetof(Record,resource),6,true},
            {"",type,"FEBCMBILJMP","System.String",offsetof(Record,name),6,true},
            {"",type,"PHFMCACHFIJ",wrong_type?"System.String":"System.Int32",offsetof(Record,id),static_field?22u:6u,true},
            {"",type,"MEPFOEEGBEA","EBMMGODFEEO.DirectorState",offsetof(Record,phase),6,true}};
    }
};
}
void TestTimelineRegistry() {
    Provider provider;const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    auto fields=[&](const char* ns,const char* type){return provider.Fields(ns,type);};
    Text path{StringTag,L"Effects/测试.playable"},name{StringTag,L"original"},private_name{StringTag,L"private"};
    Tag director=DirectorTag,director2=DirectorTag;int asset;float pos[3]{1,2,3},rot[3]{4,5,6};
    Record record{RecordTag,&director,&path,&name,21,2},record2{RecordTag,&director2,nullptr,&name,22,0};
    List list{ListTag,{&record,&record2}};Dictionary dict{DictionaryTag,{{&director,&record},{&director2,&record2}}};
    Registry registry{RegistryTag,&list,&dict};
    TimelineRegistryAccess access(provider,provider,refs,fields);
    Check(access.Find(&registry,&director)==&record && access.Find(&registry,&asset)==nullptr);
    auto rows=access.Rows(&registry);
    Check(rows.complete && rows.records.size()==2 && rows.records[0].object==&record && rows.records[1].resource_is_null);
    Check(rows.records[0].view.resource==u8"Effects/测试.playable" && rows.records[0].view.phase==TimelinePhase::Playing);
    fail_lookup=true;bool threw=false;try{access.Find(&registry,&director);}catch(...){threw=true;}fail_lookup=false;Check(threw);
    dict.items[&director]=&record2;threw=false;try{access.Find(&registry,&director);}catch(...){threw=true;}Check(threw);dict.items[&director]=&record;
    record.phase=123;rows=access.Rows(&registry);Check(!rows.complete && rows.errors.size()==1 && rows.records.size()==1 && rows.records[0].object==&record2);record.phase=2;
    Check(access.Close() && access.Close() && handles.empty());
    provider.wrong_type=true;threw=false;try{TimelineRegistryAccess bad(provider,provider,refs,fields);}catch(...){threw=true;}Check(threw);provider.wrong_type=false;
    provider.static_field=true;threw=false;try{TimelineRegistryAccess bad(provider,provider,refs,fields);}catch(...){threw=true;}Check(threw);provider.static_field=false;
    // One group tests actual native detours, all physical arguments, reentry,
    // return order and both sides of the partial-borrow ID assignment window.
    struct Receipt {TimelineRegistryAccess access;TimelineBoundaryCall call;std::optional<TimelineTicket> ticket;TimelineCallScope scope;
        Receipt(Provider& p,ReferenceMethods r,TimelineRegistryAccess::Fields f,TimelineBoundaryCall c,std::optional<TimelineOwner> owner)
            :access(p,p,r,std::move(f)),call(c),scope(owner) {}};
    TimelineOwner actor{7,8,9};TimelineOwnership ownership;
    int begins=0,ends=0,abnormal_count=0;bool rewrite=true,throw_begin=false,throw_end=false;
    std::vector<std::string> errors;std::vector<int> partial_ids;std::vector<TimelineBoundary> completed;
    TimelineRegistryHooks hooks(provider,[&](TimelineBoundaryCall& call)->void* {
        ++begins;
        if(throw_begin)throw std::runtime_error("begin fixture");
        auto receipt=std::make_unique<Receipt>(provider,refs,fields,call,actor);
        if(call.operation==TimelineBoundary::Borrow) {if(rewrite)call.argument=&private_name;}
        if(call.operation==TimelineBoundary::Return || call.operation==TimelineBoundary::Remove) {
            auto* row=call.operation==TimelineBoundary::Return?receipt->access.Find(call.registry,call.argument):call.argument;
            if(row)receipt->ticket=ownership.Watch(receipt->access.Read(call.registry,row).view,actor);
        }
        return receipt.release();
    },[&](void* raw,const TimelineBoundaryCall& call,void* result,bool abnormal) {
        ++ends;std::unique_ptr<Receipt> receipt(static_cast<Receipt*>(raw));
        if(abnormal)++abnormal_count;
        if(call.operation==TimelineBoundary::Borrow) {
            if(abnormal) {
                const auto after=receipt->access.Rows(call.registry);Check(after.complete);
                partial_ids.push_back(after.records[0].view.playback);
                Check(after.records[0].runtime_name==&private_name);
            }else {
                auto* row=receipt->access.Find(call.registry,result);Check(row==&record);
                const auto observed=receipt->access.Read(call.registry,row);
                ownership.Borrow(observed.view.key,observed.view.playback,actor);
            }
        }else if(!abnormal) {
            if(call.operation==TimelineBoundary::Return && receipt->ticket) {
                auto* row=receipt->access.Find(call.registry,call.argument);
                Check(row && receipt->access.Read(call.registry,row).view.phase==TimelinePhase::Available);
                ownership.Returned(*receipt->ticket);
            }
            if(call.operation==TimelineBoundary::Remove && receipt->ticket)ownership.Removed(*receipt->ticket);
            if(call.operation==TimelineBoundary::Dispose)ownership.RegistryDisposed(reinterpret_cast<std::uintptr_t>(call.registry));
        }
        Check(receipt->access.Close());completed.push_back(call.operation);
        if(throw_end)throw std::runtime_error("end fixture");
    },[&](const auto& e){errors.push_back(e);});
    void* result=nullptr;
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0 && begins==0 && result==&director);
    hooks.ActivateOnCurrentThread();
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0 && begins==1 && ends==1);
    Check(seen_metadata==&metadata && seen_asset==&asset && seen_position==pos && seen_rotation==rot);
    Check(record.name==&private_name && record.resource==&path && handles.empty() && !TimelineCallScope::Current());
    auto read=[&]{TimelineRegistryAccess a(provider,provider,refs,fields);auto r=a.Read(&registry,&record);Check(a.Close());return r;};
    Check(ownership.Visibility(read().view,actor)==TimelineVisibility::Owned);
    record.phase=4;
    on_return=[&]{Check(TimelineCallScope::Current()==actor && ownership.Visibility(read().view,actor)==TimelineVisibility::Owned);};
    Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0 && record.phase==0);
    Check(ownership.Visibility(read().view,std::nullopt)==TimelineVisibility::Hidden);on_return={};
    const int old_id=record.id;fail_at=1;
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0xe0760402 && record.id==old_id && !TimelineCallScope::Current());
    fail_at=2;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0xe0760403 && record.id!=old_id);
    Check(partial_ids.size()==2 && partial_ids[0]==old_id && partial_ids[1]==record.id && handles.empty());
    fail_at=0;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
    // Callback entry can recurse into another borrow. Its scope must restore to
    // the outer scope on exit, including SEH; old receipts do not clear new ones.
    on_return=[&]{Check(TimelineCallScope::Current()==actor);on_borrow=[&]{Check(TimelineCallScope::Current()==actor);};
        void* nested=nullptr;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&nested)==0);on_borrow={};};
    Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0 && !TimelineCallScope::Current());on_return={};
    // Synthetic same-record reentry is deliberately stronger than the observed
    // ordinary pool path; it verifies receipt handling, not a game behavior claim.
    for(const auto operation:{TimelineBoundary::Return,TimelineBoundary::Remove,TimelineBoundary::Dispose}) {
        fail_at=operation==TimelineBoundary::Return?3:operation==TimelineBoundary::Remove?4:5;
        Check(InvokeEnd(operation,&registry,operation==TimelineBoundary::Remove?static_cast<void*>(&record):&director)!=0);
        Check(!TimelineCallScope::Current() && handles.empty());
    }
    fail_at=0;throw_begin=true;const auto before_end=ends;
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0 && record.name==&name && ends==before_end);throw_begin=false;
    throw_end=true;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0 && !TimelineCallScope::Current());throw_end=false;
    const auto before_thread=begins;
    std::thread other([&]{Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);});other.join();Check(begins==before_thread);
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
    Check(InvokeEnd(TimelineBoundary::Remove,&registry,&record)==0 && ownership.Size()==0);
    Check(InvokeEnd(TimelineBoundary::Dispose,&registry,nullptr)==0 && list.items.empty() && dict.items.empty());
    Check(abnormal_count==5 && errors.size()==2 && handles.empty());
    hooks.Stop();const auto stopped=begins;Check(InvokeEnd(TimelineBoundary::Dispose,&registry,nullptr)==0 && begins==stopped);
    // Production tracking handlers, not a test recreation of their logic.
    list.items={&record,&record2};dict.items={{&director,&record},{&director2,&record2}};
    std::vector<std::string> tracking_log;
    PresentationClients clients;
    const auto client_a=clients.Add({},[&](const auto& line){tracking_log.push_back(line);});
    const auto client_b=clients.Add({},{});
    PresentationServiceSlot<TimelineRegistryTracking> shared_tracking;
    int installations=0;
    auto acquire=[&]{return shared_tracking.Acquire([&]{++installations;
        return std::make_shared<TimelineRegistryTracking>(provider,provider,provider,refs,fields,[&](const auto& line){clients.Log(line);});});};
    auto handle_a=acquire(),handle_b=acquire();
    Check(handle_a==handle_b && installations==1);
    auto& tracking=*handle_b;
    tracking.ActivateOnCurrentThread();
    {
        int source_world;Entity source_entity{&source_world};
        const auto token=TrackedEntities().Track(reinterpret_cast<GameEntity*>(&source_entity));
        const EntityOrigin origin{token,reinterpret_cast<GameWorld*>(&source_world)};
        const TimelineOwner source{reinterpret_cast<std::uintptr_t>(origin.world),reinterpret_cast<std::uintptr_t>(token.entity),token.generation};
        CommonTimelinePlayback playback(provider,provider,provider,refs,tracking);timeline_manager=&registry;
        auto first=playback.Start(origin,"scene",2,0);
        Check(first.call.returned && first.attributed && first.references.size()==1 && first.playback==record.id);
        auto ref=first.references.front();Check(playback.Observe(source,ref)==TimelinePlaybackPhase::Playing);
        Check(playback.Stop(source,ref).returned && record.phase==4);
        Check(playback.Observe(source,ref)==TimelinePlaybackPhase::Stopped);
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
        Check(playback.Observe(source,ref)==TimelinePlaybackPhase::Retired);
        auto second=playback.Start(origin,"scene",2,0);Check(second.attributed && second.call.returned);
        Check(playback.Stop(source,ref).returned && record.phase==2); // old request cannot stop the new borrow
        fail_after_play=true;auto interrupted=playback.Start(origin,"scene",2,0);fail_after_play=false;
        Check(!interrupted.call.returned && interrupted.attributed && interrupted.references.size()==1);
        Check(playback.Stop(source,interrupted.references.front()).returned && record.phase==4);
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
        // Failed Borrow still retains its exact private-name reference, not a
        // coincident resource or an increment guessed from the last ID.
        fail_at=1;auto partial=playback.Start(origin,"scene",2,0);fail_at=0;
        Check(!partial.call.returned && partial.attributed && partial.references.size()==1);
        Check(InvokeEnd(TimelineBoundary::Remove,&registry,&record)==0);
        list.items={&record,&record2};dict.items={{&director,&record},{&director2,&record2}};
        TrackedEntities().Disposed(token);auto dead=playback.Start(origin,"scene",2,0);
        Check(!dead.call.returned && dead.attributed && dead.references.empty());TrackedEntities().Forget(token);
        Check(playback.Close());timeline_manager=nullptr;
    }
    int world;Entity owned_entity{&world},official_entity{&world};Context owned_context{&owned_entity},official_context{&official_entity};
    Task owned_task{17,&owned_context},official_task{17,&official_context};
    const TimelineOwner task_owner{reinterpret_cast<std::uintptr_t>(&world),reinterpret_cast<std::uintptr_t>(&owned_entity),75};
    int task_before=0,task_after=0,task_abnormal=0;
    TimelineTasks tasks(provider,refs,fields,[&](GameEntity* e) {
        return e==reinterpret_cast<GameEntity*>(&owned_entity)?EntityOrigin{{e,75},reinterpret_cast<GameWorld*>(&world)}:EntityOrigin{};
    },[&](const auto&)->void* {++task_before;return nullptr;},[&](void*,const auto&,bool abnormal) {
        ++task_after;if(abnormal)++task_abnormal;
    },[&](const auto& text){tracking_log.push_back(text);});
    tasks.ActivateOnCurrentThread();
    // Actual task detour -> DLL-owned scope -> real registry handlers. No
    // manual scope around this call, nor same-module-only TLS substitute.
    on_task=[&](int index) {
        if(index==0) {
            Check(TimelineCallScope::Current()==task_owner && TimelineCallScope::Established());
            Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);record.phase=2;
        }else if(index==1) {
            Check(!TimelineCallScope::Current() && TimelineCallScope::Established());
            StopName(&registry,&name,&metadata);
        }
    };
    Check(InvokeTask(&owned_task)==0);
    // A disposed caster can still be kept by its Context. Historical source
    // world is authoritative; do not consult its now-cleared live world field.
    owned_entity.world=nullptr;Check(InvokeTask(&owned_task)==0);owned_entity.world=&world;
    Check(tracking.Ownership().Visibility(read().view,task_owner)==TimelineVisibility::Owned);
    const auto graph_source=tracking.ObserveDirector(&director);
    Check(graph_source && graph_source->owner==task_owner && graph_source->ticket.playback==record.id);
    Check(!tracking.ObserveDirector(nullptr) && !tracking.ObserveDirector(&asset));
    const auto graph_id=record.id;record.id+=97;
    Check(!tracking.ObserveDirector(&director));record.id=graph_id;
    record.phase=0;Check(!tracking.ObserveDirector(&director));record.phase=2;
    fail_lookup=true;bool graph_failed=false;
    try{tracking.ObserveDirector(&director);}catch(...){graph_failed=true;}
    fail_lookup=false;Check(graph_failed);
    Check(record.name!=&name && record.name->value==name.value);
    TaskVoid<1>(&official_task,&metadata);Check(record.phase==2);
    on_task=[&](int index) {
        if(index==0) {
            Check(TimelineCallScope::Current()==task_owner);
            TaskVoid<1>(&official_task,&metadata);
            Check(TimelineCallScope::Current()==task_owner);
            RaiseException(0xe0760421,0,0,nullptr);
        }else Check(!TimelineCallScope::Current() && TimelineCallScope::Established());
    };
    Check(InvokeTask(&owned_task)==0xe0760421 && task_abnormal==1 && !TimelineCallScope::Active());
    fail_caster=true;on_task=[&](int){Check(TimelineCallScope::Active() && !TimelineCallScope::Established());};
    Check(InvokeTask(&owned_task)==0 && !TimelineCallScope::Active());fail_caster=false;
    on_task=[&](int){Check(TimelineCallScope::Current()==task_owner);};
    TaskVoid<2>(&owned_task,&metadata);TaskTick<3>(&owned_task,0.125f,&metadata);
    Check(task_delta==0.125f && seen_metadata==&metadata);
    TaskVoid<4>(&owned_task,&metadata);TaskVoid<5>(&owned_task,&metadata);
    TaskVoid<6>(&owned_task,&metadata);TaskTick<7>(&owned_task,0.25f,&metadata);
    TaskVoid<8>(&owned_task,&metadata);TaskVoid<9>(&owned_task,&metadata);
    TaskVoid<10>(&owned_task,&metadata);TaskTick<11>(&owned_task,0.5f,&metadata);
    TaskVoid<12>(&owned_task,&metadata);TaskVoid<13>(&owned_task,&metadata);TaskVoid<14>(&owned_task,&metadata);
    Check(task_index==14 && task_before==task_after && task_delta==0.5f);
    // Nested Begin during an old Reset creates a new cycle. The old finally
    // must not delete its new source receipt, even at the same task address.
    on_task=[&](int index) {
        if(index==12) {
            owned_context.caster=&official_entity;
            TaskVoid<0>(&owned_task,&metadata);
            owned_context.caster=&owned_entity;
        }else Check(!TimelineCallScope::Current() && TimelineCallScope::Established());
    };
    TaskVoid<12>(&owned_task,&metadata);
    TaskTick<11>(&owned_task,0.2f,&metadata);
    on_task={};TaskVoid<13>(&owned_task,&metadata);
    // Every newly identified effect-query task is a real detour, not merely a
    // metadata row. Immediate scopes restore the enclosing domain and do not
    // leave weak cycle records. Same object address may be reused next Begin.
    const std::pair<int,void(__fastcall*)(Task*,void*)> immediate_effects[]{
        {15,TaskVoid<15>},{16,TaskVoid<16>},{17,TaskVoid<17>},{22,TaskVoid<22>},
        {23,TaskVoid<23>},{24,TaskVoid<24>},{25,TaskVoid<25>},{26,TaskVoid<26>},{27,TaskVoid<27>}};
    for(const auto& entry:immediate_effects) {
        Task effect_task{17,&owned_context};const auto handle_count=handles.size();
        on_task=[&](int index){Check(index==entry.first && TimelineCallScope::Established() && TimelineCallScope::Current()==task_owner);};
        entry.second(&effect_task,&metadata);Check(seen_metadata==&metadata && handles.size()==handle_count);
        effect_task.context=&official_context;
        {TimelineCallScope outer(task_owner);
            on_task=[&](int index){Check(index==entry.first && TimelineCallScope::Established() && !TimelineCallScope::Current());};
            entry.second(&effect_task,&metadata);Check(TimelineCallScope::Current()==task_owner);
            fail_caster=true;
            on_task=[&](int){Check(TimelineCallScope::Active() && !TimelineCallScope::Established());};
            entry.second(&effect_task,&metadata);fail_caster=false;Check(TimelineCallScope::Current()==task_owner);
        }
        Check(handles.size()==handle_count);
    }
    // The two persistent consumers keep caster provenance when teardown has
    // already cleared Context, and release it on normal reset/dispose.
    Task effect_task{17,&owned_context};
    on_task=[&](int){Check(TimelineCallScope::Established() && TimelineCallScope::Current()==task_owner);};
    TaskVoid<18>(&effect_task,&metadata);owned_context.caster=nullptr;
    TaskTick<19>(&effect_task,0.375f,&metadata);TaskVoid<20>(&effect_task,&metadata);
    Check(task_delta==0.375f);owned_context.caster=&owned_entity;
    TaskVoid<18>(&effect_task,&metadata);TaskVoid<21>(&effect_task,&metadata);
    TaskVoid<28>(&effect_task,&metadata);owned_context.caster=nullptr;
    TaskTick<29>(&effect_task,0.625f,&metadata);TaskVoid<30>(&effect_task,&metadata);
    Check(task_delta==0.625f);owned_context.caster=&owned_entity;
    TaskVoid<28>(&effect_task,&metadata);TaskVoid<31>(&effect_task,&metadata);
    Check(task_before==task_after && !TimelineCallScope::Active());
    const auto before_foreign=task_before;on_task={};
    std::thread task_thread([&]{TaskVoid<0>(&owned_task,&metadata);});task_thread.join();Check(task_before==before_foreign);
    Check(tasks.Close());TaskVoid<0>(&owned_task,&metadata);Check(task_before==before_foreign);
    // Existing untracked official row, owned borrow, both query directions and
    // the same first-match predicate followed by the original record stop.
    // Neither runtime-name text nor resource is namespaced.
    record2.resource=&path;record2.phase=2;record2.name=&name;
    // No private playback: official/unknown callers must reach the original
    // even if every added reflection/list observation would fail.
    fail_list_read=true;
    {TimelineCallScope unknown(std::nullopt,false);const auto before=lookup_calls;
        Check(Resource(&registry,&path,&metadata)==&record && lookup_calls==before+1);}
    fail_list_read=false;
    // The real battle log showed official records reused through an
    // unobserved borrow path. A stale public ticket must NOT hide the row.
    {TimelineCallScope official(std::nullopt);
        Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);}
    ++record.id;record.phase=2;
    Check(Resource(&registry,&path,&metadata)==&record);
    StopName(&registry,&name,&metadata);Check(record.phase==4 && record2.phase==2);
    {
        TimelineCallScope owned(actor);
        Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);record.phase=2;
        const auto before=lookup_calls;
        Check(Resource(&registry,&path,&metadata)==&record && lookup_calls==before+1);
        {TimelineCallScope official(std::nullopt);Check(Resource(&registry,&path,&metadata)==&record2);}
        {TimelineCallScope unknown(std::nullopt,false);Check(Resource(&registry,&path,&metadata)==&record2);}
        Check(Resource(&registry,&path,&metadata)==&record);
        StopName(&registry,&name,&metadata);
        Check(record.phase==4 && record2.phase==2 && seen_metadata==&metadata);
        Check(record.name->value==name.value);
    }
    Check(Resource(&registry,&path,&metadata)==&record2);
    // Complete-view failures never publish a partial/empty public result.
    // Preserve the engine's outcome; known private callers do not fall through
    // onto the official records. The shared registry/list stay unchanged.
    for(int failure=0;failure<3;++failure) {
        fail_list_read=failure==0;fail_list_item=failure==1?1:-1;fail_query_write=failure==2;
        const auto before=lookup_calls;
        Check(Resource(&registry,&path,&metadata)==&record2 && lookup_calls==before+1);
        {TimelineCallScope owned(actor);const auto count=lookup_calls;
            Check(Resource(&registry,&path,&metadata)==nullptr && lookup_calls==count);}
        Check(registry.list==&list && list.items==std::vector<Record*>({&record,&record2}));
        fail_list_read=false;fail_list_item=-1;fail_query_write=false;
    }
    StopName(&registry,&name,&metadata);Check(record2.phase==4 && record.phase==4);
    // Same-domain duplicate names preserve first-match semantics, including
    // Preparing: do not skip it and stop the next already-playing row.
    record2.phase=1;
    Tag stop_director=DirectorTag;Record stop_later{RecordTag,&stop_director,&path,&name,899,2};
    list.items.push_back(&stop_later);StopName(&registry,&name,&metadata);
    Check(record2.phase==1 && stop_later.phase==2);list.items.pop_back();record2.phase=2;
    fail_record_stop=true;const auto before_stop=stop_calls;
    Check(InvokeEnd(TimelineBoundary::StopName,&registry,&name)==0xe0760423 && stop_calls==before_stop+1);
    fail_record_stop=false;record2.phase=4;
    // Asset name is different from both the runtime name and resource path.
    // Public and owned rows share that asset, but each sees only its domain.
    Text asset_name{StringTag,L"视觉资产"};
    director_assets={{&director,&asset_name},{&director2,&asset_name}};
    director_states={{&director,1},{&director2,0}};
    const auto before_playing=playing_calls;
    Check(!AssetPlaying(&registry,&asset_name,&metadata));
    {
        TimelineCallScope owned(actor);
        Check(AssetPlaying(&registry,&asset_name,&metadata));
        Check(!AssetPlaying(&registry,&name,&metadata) && !AssetPlaying(&registry,&path,&metadata));
        {TimelineCallScope unknown(std::nullopt,false);Check(!AssetPlaying(&registry,&asset_name,&metadata));}
        Check(AssetPlaying(&registry,&asset_name,&metadata));
    }
    Check(playing_calls==before_playing+6); // all decisions execute the original matcher
    // Official List.Find selects the first asset-name match, then tests the
    // Director. A later playing match does not turn the result true.
    Tag director3=DirectorTag;Record record3{RecordTag,&director3,&path,&name,900,2};
    director_assets[&director3]=&asset_name;director_states[&director3]=1;list.items.push_back(&record3);
    Check(!AssetPlaying(&registry,&asset_name,&metadata));
    director_states[&director2]=1;Check(AssetPlaying(&registry,&asset_name,&metadata));
    list.items.pop_back();director_assets.erase(&director3);director_states.erase(&director3);
    // Null is invalid for a nonempty official list, not a null-resource match.
    // Empty text remains an ordinary valid lookup.
    Text empty{StringTag,L""};record2.resource=nullptr;
    const auto before_null=lookup_calls;
    Check(InvokeResource(&registry,nullptr,&result)==0xe0760422 && lookup_calls==before_null+1);
    Check(Resource(&registry,&empty,&metadata)==nullptr);
    record2.resource=&empty;
    Check(Resource(&registry,&empty,&metadata)==&record2 && InvokeResource(&registry,nullptr,&result)==0xe0760422);
    list.items.clear();Check(InvokeResource(&registry,nullptr,&result)==0 && !result);
    list.items={nullptr};Check(InvokeResource(&registry,nullptr,&result)==0 && !result);
    list.items={&record,&record2};
    record2.resource=nullptr;record2.phase=0;
    {
        TimelineCallScope owned(actor);
        Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
        Check(record.name!=&name && record.resource==&path && tracking.Ownership().Visibility(read().view,actor)==TimelineVisibility::Owned);
        Check(record.name->value==name.value);
        record.phase=4;
        on_return=[&]{Check(TimelineCallScope::Current()==actor && TimelineCallScope::Established());};
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);on_return={};
        Check(tracking.Ownership().Visibility(read().view,std::nullopt)==TimelineVisibility::Hidden);
        // Incomplete borrow is still private, but never published as successful.
        for(int failure:{1,2}) {
            fail_at=failure;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)!=0);
            Check(tracking.Ownership().Visibility(read().view,actor)==TimelineVisibility::Hidden);
            Check(tracking.Ownership().Visibility(read().view,std::nullopt)==TimelineVisibility::Hidden);
        }
        fail_at=0;Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
        const auto key=read().view.key;const auto ticket=tracking.Ownership().Ticket(key);Check(ticket.has_value());
        fail_at=3;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)!=0);fail_at=0;
        Check(tracking.Ownership().Ticket(key)->borrow==ticket->borrow && tracking.Ownership().Visibility(read().view,actor)==TimelineVisibility::Owned);
        record.phase=4;
        on_return=[&] {
            std::swap(list.items[0],list.items[1]);void* nested=nullptr;
            Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&nested)==0 && nested==&director2);
            std::swap(list.items[0],list.items[1]);
        };
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);on_return={};
        TimelineRegistryAccess nested_read(provider,provider,refs,fields);
        const auto callback_row=nested_read.Read(&registry,&record2);Check(nested_read.Close());
        Check(tracking.Ownership().Visibility(callback_row.view,actor)==TimelineVisibility::Owned);
        Check(InvokeEnd(TimelineBoundary::Remove,&registry,&record2)==0);
        list.items.push_back(&record2);dict.items[&director2]=&record2; // later unknown-callback case
    }
    Check(!TimelineCallScope::Established());
    // The shared list is never swapped, even INSIDE callbacks. Original return
    // writes into a per-owner view before invoking the completion callback.
    Text public_a{StringTag,L"official/a"},public_b{StringTag,L"official/b"};
    Text owned_a{StringTag,L"owned/a"},owned_b{StringTag,L"owned/b"},owned_c{StringTag,L"owned/c"};
    Text query_owned{StringTag,L"owned/"},query_public{StringTag,L"official/"},query_namespace{StringTag,L"hksr/"};
    official_history.items={{&public_a,100},{&public_b,100}};history_frame=100;
    const auto original_history=official_history.items;
    auto complete=[&](TimelineOwner owner,Text& title) {
        TimelineCallScope scope(owner);
        Check(InvokeBorrow(&registry,&title,&path,&asset,pos,rot,&result)==0);record.phase=4;
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
    };
    on_return=[&] {
        Check(registry.history==&official_history && official_history.items==original_history);
        Check(Recent(&registry,&query_owned,0,&metadata));
        Check(!Recent(&registry,&query_public,0,&metadata));
        {TimelineCallScope official(std::nullopt);Check(Recent(&registry,&query_public,0,&metadata));Check(!Recent(&registry,&query_owned,0,&metadata));}
    };
    complete(actor,owned_a);complete(actor,owned_b);complete(actor,owned_c);on_return={};
    // The FIRST client's handle and callbacks can retire while shared native
    // boundaries still own its history. A third client must reuse these exact
    // detours, not overwrite or fail to install another set.
    clients.Detach(client_a);handle_a.reset();
    const auto client_c=clients.Add({},[&](const auto& line){tracking_log.push_back(line);});
    auto handle_c=acquire();Check(handle_c==handle_b && installations==1);
    Check(official_history.items==original_history && registry.history==&official_history);
    {
        TimelineCallScope scope(actor);
        Check(!Recent(&registry,&owned_a,0,&metadata) && Recent(&registry,&owned_b,0,&metadata) && Recent(&registry,&owned_c,0,&metadata));
        Check(!Recent(&registry,&query_namespace,0,&metadata) && Recent(&registry,&empty,0,&metadata));
        ++history_frame;Check(!Recent(&registry,&query_owned,0,&metadata) && Recent(&registry,&query_owned,1,&metadata));
        bool recent_result=false;Check(InvokeRecent(&registry,nullptr,1,&recent_result)==0xe0760425);
        // Failure after append does not undo the official early history write.
        Check(InvokeBorrow(&registry,&owned_a,&path,&asset,pos,rot,&result)==0);record.phase=4;fail_at=3;
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0xe0760404);fail_at=0;
        Check(Recent(&registry,&owned_a,0,&metadata) && !Recent(&registry,&owned_b,1,&metadata));
    }
    auto other_owner=actor;++other_owner.entity;complete(other_owner,owned_b);
    {TimelineCallScope scope(other_owner);Check(!Recent(&registry,&owned_a,1,&metadata) && Recent(&registry,&owned_b,1,&metadata));}
    auto reborn=actor;++reborn.lifetime;
    {TimelineCallScope scope(reborn);Check(!Recent(&registry,&query_owned,1,&metadata));}
    auto other_world=actor;++other_world.world;
    {TimelineCallScope scope(other_world);Check(!Recent(&registry,&query_owned,1,&metadata));}
    {TimelineCallScope unknown(std::nullopt,false);Check(Recent(&registry,&query_public,1,&metadata));}
    Check(official_history.items==original_history);
    // Nested official return updates the REAL history, not the outer private
    // view; the private callback can immediately query both isolated domains.
    record2.name=&public_a;record2.phase=4;
    bool nested_return=false;
    on_return=[&] {
        if(nested_return)return;
        nested_return=true;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director2)==0);
        Check(registry.history==&official_history && official_history.items.back().first==&public_a);
        Check(Recent(&registry,&owned_c,0,&metadata));
    };
    complete(actor,owned_c);on_return={};
    // Empty names are retained as empty, so Return must not manufacture history.
    {TimelineCallScope scope(actor);Check(Recent(&registry,&owned_c,0,&metadata));}
    complete(actor,empty);complete(actor,empty);
    {TimelineCallScope scope(actor);Check(Recent(&registry,&owned_c,0,&metadata));}
    // A projection failure has no live-registry write. Existing fail-open policy
    // forwards the original query, and is explicitly NOT strict isolation.
    const auto before_failed_projection=official_history.items;
    fail_history_write=true;
    {TimelineCallScope scope(actor);Check(Recent(&registry,&query_public,1,&metadata));}
    fail_history_write=false;Check(official_history.items==before_failed_projection && registry.history==&official_history);
    Check(tracking.CollectHistories([](TimelineOwner){return true;})==0);
    {
        TimelineCallScope scope(actor);
        Check(InvokeBorrow(&registry,&owned_a,&path,&asset,pos,rot,&result)==0);record.phase=2;
    }
    Check(tracking.Ownership().HasPending(actor));
    // Retired/collected source is insufficient while its Director is pending.
    tracking.CollectHistories([](TimelineOwner){return false;});
    {TimelineCallScope scope(actor);Check(Recent(&registry,&owned_c,1,&metadata));}
    record.phase=4;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
    Check(!tracking.Ownership().HasPending(actor));
    Check(tracking.CollectHistories([](TimelineOwner){return false;})==1);
    {TimelineCallScope scope(actor);Check(!Recent(&registry,&query_owned,1,&metadata));}
    // Unknown source cannot erase an old marker. A subsequent positively known
    // official borrow can; this is deliberately not "no scope means official".
    Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
    Check(tracking.Ownership().Visibility(read().view,std::nullopt)==TimelineVisibility::StaleObservation);
    {TimelineCallScope official(std::nullopt);Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);}
    Check(tracking.Ownership().Visibility(read().view,std::nullopt)==TimelineVisibility::Public && !handles.empty());
    {
        TimelineCallScope owned(actor);
        // A positively observed public borrow establishes public callback scope.
        on_return=[&]{Check(TimelineCallScope::Established() && !TimelineCallScope::Current());};
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);on_return={};
        Check(TimelineCallScope::Current()==actor);
        // The second, untracked record has UNKNOWN origin, not inherited actor.
        on_return=[&]{Check(!TimelineCallScope::Established() && !TimelineCallScope::Current());};
        Check(InvokeEnd(TimelineBoundary::Return,&registry,&director2)==0);on_return={};
        Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
        Check(InvokeEnd(TimelineBoundary::Remove,&registry,&record)==0 && tracking.Ownership().Size()==0);
        list.items.insert(list.items.begin(),&record);dict.items[&director]=&record;
        Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);
        Check(InvokeEnd(TimelineBoundary::Dispose,&registry,nullptr)==0 && tracking.Ownership().Size()==0);
    }
    // Connect real task scopes, registry filtering and saved-record adapter.
    // The synthetic original below models only audited branch boundaries; it
    // does not certify the game's camera timings or task scheduler as a whole.
    list.items={&record,&record2};dict.items={{&director,&record},{&director2,&record2}};
    record.phase=0;record2.phase=0;record2.resource=&path;record2.name=&name;
    TimelineWaits waits(provider,provider,provider,refs,fields,tracking,[&](const auto& s){tracking_log.push_back(s);});
    TimelineTasks waiting_tasks(provider,refs,fields,[&](GameEntity* e) {
        return e==reinterpret_cast<GameEntity*>(&owned_entity)?EntityOrigin{{e,75},reinterpret_cast<GameWorld*>(&world)}:EntityOrigin{};
    },[&](const auto& call){return waits.Before(call);},[&](void* p,const auto& call,bool abnormal){waits.After(p,call,abnormal);},
        [&](const auto& text){tracking_log.push_back(text);});
    waiting_tasks.ActivateOnCurrentThread();unsigned camera_frame=100;bool closeup_pending=false;
    on_task=[&](int index) {
        auto& t=*running_task;
        if(index==0){Check(InvokeBorrow(&registry,&name,&path,&asset,pos,rot,&result)==0);record.phase=2;return;}
        if(index==1){StopName(&registry,&name,&metadata);return;}
        if(index==4 || index==8){t.saved=nullptr;t.state=0;t.camera=0;return;}
        if(index==5 || index==9)return;
        if(index==2 || index==6) {
            t.saved=static_cast<Record*>(Resource(&registry,&path,&metadata));t.state=1;
            if(index==6 && (!t.saved || (t.saved->phase==4 && closeup_pending))) {t.saved=nullptr;t.camera=camera_frame;return;}
            if(!t.saved || t.saved->phase==0 || t.saved->phase==4)t.state=2;
            return;
        }
        if(t.state!=1)return;
        if(index==7 && !t.saved) {
            if(t.camera==camera_frame)return;
            t.saved=static_cast<Record*>(Resource(&registry,&path,&metadata));
        }
        if(!t.saved || t.saved->phase==0 || t.saved->phase==4)t.state=2;
    };
    TaskVoid<0>(&owned_task,&metadata);TaskVoid<6>(&owned_task,&metadata);
    Check(owned_task.saved==&record && owned_task.state==1);
    // OnTaskBegin established ownership. Later teardown may clear Context's
    // caster; the same task cycle must retain origin rather than turn public.
    owned_context.caster=nullptr;
    TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.saved==&record);
    Check(waiting_tasks.HasSource(task_owner));owned_context.caster=&owned_entity;
    record.phase=4;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
    TaskVoid<0>(&owned_task,&metadata); // same owner, same pooled object, new play
    owned_task.camera=camera_frame; // exercise a still-equal counter after detach
    TaskTick<7>(&owned_task,0.1f,&metadata);
    Check(owned_task.state==1 && owned_task.saved==nullptr && record.phase==2);
    ++camera_frame;TaskTick<7>(&owned_task,0.1f,&metadata);
    Check(owned_task.state==2 && owned_task.saved==nullptr && record.phase==2);
    TaskVoid<8>(&owned_task,&metadata);TaskVoid<6>(&owned_task,&metadata);
    Check(owned_task.saved==&record && owned_task.state==1); // reset allows NEW wait
    TaskVoid<9>(&owned_task,&metadata);
    // Official waiter cannot follow a record subsequently reused privately.
    TaskVoid<0>(&official_task,&metadata);TaskVoid<2>(&official_task,&metadata);
    Check(official_task.saved==&record && official_task.state==1);
    record.phase=4;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
    TaskVoid<0>(&owned_task,&metadata);TaskTick<3>(&official_task,0.1f,&metadata);
    Check(official_task.saved==nullptr && official_task.state==2 && record.phase==2);
    TaskVoid<5>(&official_task,&metadata);
    // A failed/absent caster on an OFFICIAL task must not inherit the outer
    // actor or swallow its own query/stop. The private same-name row survives.
    record.phase=2;record2.phase=2;official_task.context=nullptr;
    {TimelineCallScope outer(task_owner);TaskVoid<2>(&official_task,&metadata);
        Check(official_task.saved==&record2 && official_task.state==1);
        TaskVoid<1>(&official_task,&metadata);Check(record2.phase==4 && record.phase==2);}
    official_task.context=&official_context;TaskVoid<5>(&official_task,&metadata);
    // Pure official reuse is not a reason for our adapter to clear the saved
    // reference or override the game's own Tick/camera transition semantics.
    TaskVoid<0>(&official_task,&metadata);TaskVoid<2>(&official_task,&metadata);
    Check(official_task.saved==&record && official_task.state==1);
    record.phase=4;Check(InvokeEnd(TimelineBoundary::Return,&registry,&director)==0);
    TaskVoid<0>(&official_task,&metadata);TaskTick<3>(&official_task,0.1f,&metadata);
    Check(official_task.saved==&record && official_task.state==1);TaskVoid<5>(&official_task,&metadata);
    // Initial null and stopped-with-pending-camera cases preserve official
    // counter delay AND the later first lookup (not marked exhausted early).
    record.phase=0;record2.phase=0;
    TaskVoid<6>(&owned_task,&metadata);Check(owned_task.saved==nullptr && owned_task.state==1 && owned_task.camera==camera_frame);
    TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.state==1);
    TaskVoid<0>(&owned_task,&metadata);++camera_frame;
    TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.saved==&record && owned_task.state==1);
    record.phase=4;closeup_pending=true;
    TaskVoid<6>(&owned_task,&metadata);Check(!owned_task.saved && owned_task.state==1);
    TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.state==1);
    ++camera_frame;TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.state==2);
    TaskVoid<9>(&owned_task,&metadata);closeup_pending=false;
    // Pre-install official saved reference which was not queried by candidate:
    // private reuse is still rejected at the first observed official Tick.
    official_task.saved=&record;official_task.state=1;
    TaskVoid<0>(&owned_task,&metadata);TaskTick<3>(&official_task,0.1f,&metadata);
    Check(!official_task.saved && official_task.state==2);TaskVoid<5>(&official_task,&metadata);
    // An abandoned task need not reach Reset/Dispose. Our wait registry must
    // not keep it alive; simulate GC only after all native calls returned.
    TaskVoid<6>(&owned_task,&metadata);
    CollectFixtureObject(&owned_task);waits.Collect();waiting_tasks.Collect();
    Check(!waiting_tasks.HasSource(task_owner));
    for(const auto& h:handles)Check(h.second!=&owned_task);
    // Reuse exactly that address for a fresh task without inheriting exhausted
    // state or the old wait ticket. Begin and first-seen Tick both support it.
    owned_task.saved=&record;owned_task.state=1;
    TaskTick<7>(&owned_task,0.1f,&metadata);Check(owned_task.saved==&record);
    TaskVoid<9>(&owned_task,&metadata);
    on_task={};Check(waiting_tasks.Close());Check(waits.Close());
    Check(InvokeEnd(TimelineBoundary::Dispose,&registry,nullptr)==0);
    Check(tracking.Close() && !tracking_log.empty() && !TimelineCallScope::Established() && handles.empty());
    clients.Detach(client_b);clients.Detach(client_c);
    // Failed GC decrements are reported and never retried by Close/destruction.
    registry.list=&list;TimelineRegistryAccess cleanup(provider,provider,refs,fields);cleanup.Rows(&registry);
    fail_free=true;const auto frees_before=free_calls;Check(!cleanup.Close());const auto frees_after=free_calls;
    Check(frees_after>frees_before && !cleanup.Close() && free_calls==frees_after);fail_free=false;
    handles.clear();handle_kinds.clear(); // synthetic fixture only
    registry_views.clear();private_histories.clear();official_history.items.clear();
    std::cout<<"Timeline candidate passed: real task/registry detours, original-name scoped stop, independent completion histories, callback/exception ordering, wait reuse and camera-delay branches, ABI/SEH/reentry and GC receipts. Local fixtures, not game adoption.\n";
}
