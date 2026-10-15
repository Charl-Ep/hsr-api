#include "../../../src/presentation/effects/EffectQueries.hpp"
#include "../../../src/presentation/effects/EffectScopes.hpp"
#include <Windows.h>
#include <cstddef>
#include <map>
#include <thread>
#include <iostream>

namespace {
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
void CheckAt(bool value,int line){if(!value)throw std::runtime_error("effect query boundary test failed at line "+std::to_string(line));}
#define Check(value) CheckAt((value),__LINE__)
enum Tag {ManagerTag,ListTag,FieldTag,TypeTag,TextTag};
struct Effect;
struct Request {void* caster=nullptr;void* target=nullptr;void* parameter=nullptr;Effect* instance=nullptr;unsigned id=0;bool released=false;int alive=0;};
struct Effect {Request* request=nullptr;std::wstring name;void* behavior_target=nullptr;bool native_alive=true;};
struct Behavior {Effect* owner=nullptr;};
struct Plugin {int padding=34;Effect* owner=nullptr;};
struct CameraController {Plugin* plugin=nullptr;bool enabled=false;int subscriptions=0,writes=0;};
struct List {Tag tag=ListTag;std::vector<Effect*> rows;};
struct Manager {Tag tag=ManagerTag;List* list=nullptr;int unrelated=123;};
struct Text {Tag tag=TextTag;std::wstring value;};
Tag field_object=FieldTag,type_object=TypeTag;
int method_info=1,generic_info=2;
std::map<int,void*> handles;int next_handle=0;
std::vector<std::unique_ptr<List>> lists;
std::vector<std::unique_ptr<Manager>> views;
std::vector<std::unique_ptr<Text>> texts;
Manager* real_manager=nullptr;
void* seen_name=nullptr;void* seen_target=nullptr;void* seen_info=nullptr;Manager* seen_receiver=nullptr;
bool throw_original=false,throw_copy=false,throw_field=false,throw_release=false;
int free_calls=0;
std::function<void()> nested;
std::function<void(int)> in_effect;
bool throw_owner=false;float seen_delta=0;Effect* seen_effect_owner=nullptr;
bool throw_camera=false,throw_camera_getter=false;
int camera_info=71,camera_update_info=72,camera_gets=0;
void* __fastcall CameraPlugin(CameraController* c,void*) {
    ++camera_gets;
    if(throw_camera_getter)RaiseException(0xe0753020,0,0,nullptr);
    return c->plugin;
}
__declspec(noinline) void __fastcall CameraEnable(CameraController* c,bool enabled,void* info) {
    Check(info==&camera_info);
    if(throw_camera)RaiseException(0xe0753021,0,0,nullptr);
    if(c->enabled!=enabled)c->subscriptions+=enabled?1:-1;
    c->enabled=enabled;
}
__declspec(noinline) void __fastcall CameraUpdate(CameraController* c,void* info) {
    Check(info==&camera_update_info);
    CameraEnable(c,true,&camera_info);
    ++c->writes; // deliberately no enabled check, matching the audited body
}
DWORD InvokeCamera(CameraController* c) {
    __try{CameraUpdate(c,&camera_update_info);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void* __fastcall OwnerMono(Behavior* b,void*) {
    if(throw_owner)RaiseException(0xe0753014,0,0,nullptr);
    return b->owner;
}
template<int Index>__declspec(noinline) void __fastcall EffectVoid(void* object,void* info) {
    Check(object && info==&method_info);
    // Make the fixture's contents observably consumed by the native original.
    // Merely checking object!=null lets the optimizer elide/reuse its storage;
    // a detour's hidden memory reads are not visible to that optimization.
    if constexpr(Index==4)seen_effect_owner=static_cast<Plugin*>(object)->owner;
    else seen_effect_owner=static_cast<Behavior*>(object)->owner;
    if(in_effect)in_effect(Index);
}
__declspec(noinline) void __fastcall EffectTick(void* object,float delta,void* info) {
    seen_delta=delta;EffectVoid<3>(object,info);
}
DWORD InvokeEffect(void* object) {
    __try{EffectVoid<1>(object,&method_info);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<class F>Method M(F f,void* info=&method_info){return {reinterpret_cast<void*>(f),info};}
int __fastcall Keep(void* object,int,int kind,void*){Check(kind==2);handles[++next_handle]=object;return next_handle;}
void* __fastcall Target(int id,void*){return handles.at(id);}
void __fastcall Free(int id,void*){++free_calls;if(throw_release)RaiseException(0xe0753011,0,0,nullptr);handles.erase(id);}
int __fastcall Count(List* list,void* info){Check(info==&generic_info);return static_cast<int>(list->rows.size());}
void* __fastcall Item(List* list,int index,void* info){Check(info==&generic_info);return list->rows.at(index);}
void* __fastcall Range(List* list,int start,int count,void* info) {
    Check(info==&generic_info);
    if(throw_copy)RaiseException(0xe0753012,0,0,nullptr);
    auto copy=std::make_unique<List>();copy->rows.assign(list->rows.begin()+start,list->rows.begin()+start+count);
    lists.push_back(std::move(copy));return lists.back().get();
}
void __fastcall Remove(List* list,int index,void* info) {
    Check(info==&generic_info && list!=real_manager->list);list->rows.erase(list->rows.begin()+index);
}
void* __fastcall Clone(Manager* manager,void*) {
    views.push_back(std::make_unique<Manager>(*manager));return views.back().get();
}
void* __fastcall GetField(void* type,Text* text,int flags,void*) {
    Check(type==&type_object && text->value==L"_EffectList" && flags==54);return &field_object;
}
void __fastcall SetField(void* field,Manager* manager,List* list,int flags,void* binder,void* culture,void*) {
    Check(field==&field_object && manager!=real_manager && flags==0 && !binder && !culture);
    if(throw_field)RaiseException(0xe0753013,0,0,nullptr);
    manager->list=list;
}
// Synthetic original implements only the audited ordinary selection contract.
// The adapter must use this body, not silently substitute its own name/target
// matching. Artificial native exceptions test propagation, not game error IDs.
__declspec(noinline) void* __fastcall Find(Manager* manager,Text* name,void* target,void* info) {
    seen_receiver=manager;seen_name=name;seen_target=target;seen_info=info;
    Check(manager->unrelated==123);
    if(nested){auto callback=std::move(nested);nested={};callback();}
    if(throw_original)RaiseException(0xe0753010,0,0,nullptr);
    if(!manager->list || !name || name->value.empty())return nullptr;
    for(auto* effect:manager->list->rows) {
        if(!effect || !effect->native_alive)continue;
        if(target && effect->behavior_target!=target)continue;
        if(effect->name!=name->value || !effect->request || effect->request->alive>4)continue;
        return effect;
    }
    return nullptr;
}
DWORD Invoke(Manager* manager,Text* name,void* target,void** result) {
    __try{*result=Find(manager,name,target,&method_info);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Provider:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char*,const char*,const char* name,const char*,std::initializer_list<const char*>,MethodKind)override {
        const std::string key=name;
        if(key=="FindEffectByUniqueName")return M(&Find);
        if(key=="MemberwiseClone")return M(&Clone);
        if(key=="GetField")return M(&GetField);
        if(key=="get_OwnerMono")return M(&OwnerMono);
        if(key=="PreResetPlugins")return M(EffectVoid<0>);
        if(key=="ResetPlugins")return M(EffectVoid<1>);
        if(key=="DisposePlugins")return M(EffectVoid<2>);
        if(key=="Tick")return M(EffectTick);
        if(key=="EFPBCKGBFBG")return M(EffectVoid<4>);
        if(key=="HPNDIIPEFLJ")return M(CameraPlugin);
        if(key=="NMIPDBNGLBH")return M(CameraEnable,&camera_info);
        if(key=="BLOMJCEJLFG")return M(CameraUpdate,&camera_update_info);
        throw std::runtime_error("unexpected effect query method: "+key);
    }
    DeclaredType ReadObjectType(void* object)override {
        if(*static_cast<Tag*>(object)==ListTag)return {"System.Collections.Generic","List<MonoEffect>",{
            {"get_Count","System.Int32",{},false,M(&Count,&generic_info)},
            {"get_Item","RPG.Client.MonoEffect",{"System.Int32"},false,M(&Item,&generic_info)},
            {"GetRange","System.Collections.Generic.List<RPG.Client.MonoEffect>",{"System.Int32","System.Int32"},false,M(&Range,&generic_info)},
            {"RemoveAt","System.Void",{"System.Int32"},false,M(&Remove,&generic_info)}}};
        Check(object==&field_object);
        return {"System.Reflection","MonoField",{{"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(&SetField)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void*)override{throw std::runtime_error("unexpected field query");}
    ObjectResult NewStringUtf8(const char* text)override {
        auto value=std::make_unique<Text>();while(*text)value->value+=static_cast<wchar_t>(*text++);
        texts.push_back(std::move(value));return {{true,0},texts.back().get()};
    }
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("must not construct manager");}
    ObjectResult ManagedType(const char* space,const char* name)override {
        Check(std::string(space)=="RPG.Client" && std::string(name)=="MonoEffectManager");return {{true,0},&type_object};
    }
};
std::vector<DeclaredField> Fields(const char*,const char* owner) {
    auto field=[&](const char* name,const char* type,std::size_t offset){return DeclaredField{"RPG.Client",owner,name,type,offset,6,true};};
    const std::string type=owner;
    if(type=="MonoEffectManager")return {field("_EffectList","System.Collections.Generic.List<RPG.Client.MonoEffect>",offsetof(Manager,list))};
    if(type=="MonoEffect")return {field("TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request))};
    if(type=="MonoEffectPluginBase")return {field("_OwnerEffect","RPG.Client.MonoEffect",offsetof(Plugin,owner))};
    Check(type=="TriggerEffectParams");return {
        field("CasterEntity","RPG.GameCore.GameEntity",offsetof(Request,caster)),
        field("TargetEntity","RPG.GameCore.GameEntity",offsetof(Request,target)),
        field("ParamEntity","RPG.GameCore.GameEntity",offsetof(Request,parameter)),
        field("<EffectInstance>k__BackingField","RPG.Client.MonoEffect",offsetof(Request,instance)),
        field("<UniqueEffectID>k__BackingField","System.UInt32",offsetof(Request,id)),
        field("<IsReleased>k__BackingField","System.Boolean",offsetof(Request,released))};
}
}
void TestEffectQueries() {
    Provider provider;ReferenceMethods refs{M(&Keep),M(&Target),M(&Free),{}, {}};
    int entity_a,entity_b,entity_public,target_a,target_b;
    const TimelineOwner a{1,reinterpret_cast<std::uintptr_t>(&entity_a),1},b{1,reinterpret_cast<std::uintptr_t>(&entity_b),2};
    Request ra,rb,rp;ra.caster=&entity_a;rb.caster=&entity_b;rp.caster=&entity_public;
    ra.target=rb.target=rp.target=&target_a;ra.id=0;rb.id=1;rp.id=2;
    Effect ea{&ra,L"same",&target_a},eb{&rb,L"same",&target_a},ep{&rp,L"same",&target_a};
    ra.instance=&ea;rb.instance=&eb;rp.instance=&ep;
    List list{ListTag,{&eb,nullptr,&ea,&ep}};Manager manager{ManagerTag,&list};real_manager=&manager;
    Text query{TextTag,L"same"},empty{TextTag,L""};std::vector<std::string> logs;
    bool bad_source=false,change_generation=false,owned_null_request=false;
    const auto source=[&](EffectReference ref,const EffectRelationsSnapshot& facts)->std::optional<TimelineOwner> {
            if(bad_source)throw std::runtime_error("unknown source");
            if(change_generation && ref.parameters==&ra)++ra.id;
            if(owned_null_request && ref.parameters==&ra && ref.id==ra.id)return a;
            if(facts.caster.object==&entity_a)return a;
            if(facts.caster.object==&entity_b)return b;
            if(facts.caster.object==&entity_public)return {};
            throw std::runtime_error("null caster is not target ownership");
        };
    EffectQueries adapter(provider,provider,provider,refs,Fields,source,[&](const auto& text){logs.push_back(text);});
    // Inactive and missing scope calls remain original, not implicitly public.
    Check(Find(&manager,&query,nullptr,&method_info)==&eb);adapter.ActivateOnCurrentThread();
    Check(Find(&manager,&query,nullptr,&method_info)==&eb && !logs.empty());
    {
        TimelineCallScope owned(a);
        Check(Find(&manager,&query,nullptr,&method_info)==&ea);
        Check(seen_receiver!=&manager && seen_name==&query && !seen_target && seen_info==&method_info);
        Check(manager.list==&list && list.rows==std::vector<Effect*>({&eb,nullptr,&ea,&ep}));
        Check(Find(&manager,&query,&target_a,&method_info)==&ea && seen_target==&target_a);
        Check(Find(&manager,&query,&target_b,&method_info)==nullptr); // target not replaced with caster
        ea.behavior_target=&target_b; // original reads Behavior, not request target
        Check(Find(&manager,&query,&target_a,&method_info)==nullptr);
        Check(Find(&manager,&query,&target_b,&method_info)==&ea);ea.behavior_target=&target_a;
        ea.native_alive=false;Check(!Find(&manager,&query,nullptr,&method_info));ea.native_alive=true;
        ra.alive=5;Check(!Find(&manager,&query,nullptr,&method_info));ra.alive=4;
        ra.released=true;Check(Find(&manager,&query,nullptr,&method_info)==&ea);ra.released=false; // no invented released filter
        Request second_request=ra;second_request.id=3;
        Effect second_a{&second_request,L"same",&target_a};second_request.instance=&second_a;
        list.rows={&eb,&second_a,&ea,&ep};
        Check(Find(&manager,&query,nullptr,&method_info)==&second_a); // first retained match, not last
        list.rows={&eb,nullptr,&ea,&ep};
        Check(!Find(&manager,nullptr,nullptr,&method_info) && !Find(&manager,&empty,nullptr,&method_info));
        nested=[&] {
            TimelineCallScope public_scope(std::nullopt);
            Check(Find(&manager,&query,nullptr,&method_info)==&ep);
        };
        Check(Find(&manager,&query,nullptr,&method_info)==&ea);
        Check(TimelineCallScope::Current()==a);
        throw_original=true;void* output=nullptr;
        Check(Invoke(&manager,&query,nullptr,&output)==0xe0753010);throw_original=false;
        Check(handles.empty() && manager.list==&list && list.rows.size()==4);
        bad_source=true;Check(Find(&manager,&query,nullptr,&method_info)==&eb);bad_source=false;
        change_generation=true;Check(Find(&manager,&query,nullptr,&method_info)==&eb);change_generation=false;
        ra.caster=nullptr;Check(Find(&manager,&query,nullptr,&method_info)==&eb);ra.caster=&entity_a;
        throw_copy=true;Check(Find(&manager,&query,nullptr,&method_info)==&eb);throw_copy=false;
        throw_field=true;Check(Find(&manager,&query,nullptr,&method_info)==&eb);throw_field=false;
        Check(handles.empty()); // failed preparation releases its partial inputs
        {
            TimelineCallScope unknown(std::nullopt,false);
            Check(Find(&manager,&query,nullptr,&method_info)==&eb);
        }
        {
            TimelineCallScope rebuilt(TimelineOwner{a.world,a.entity,a.lifetime+1});
            Check(!Find(&manager,&query,nullptr,&method_info));
        }
        {
            TimelineCallScope other_world(TimelineOwner{a.world+1,a.entity,a.lifetime});
            Check(!Find(&manager,&query,nullptr,&method_info));
        }
        void* other=nullptr;std::thread worker([&]{other=Find(&manager,&query,nullptr,&method_info);});worker.join();Check(other==&eb);
    }
    {
        TimelineCallScope public_scope(std::nullopt);
        Check(Find(&manager,&query,nullptr,&method_info)==&ep); // reverse direction isolated
        list.rows={&ep,nullptr};const auto copies=lists.size();
        Check(Find(&manager,&query,nullptr,&method_info)==&ep && seen_receiver==&manager && lists.size()==copies);
        list.rows.clear();Check(!Find(&manager,&query,nullptr,&method_info));
    }
    list.rows={&eb,&ea,&ep};
    // Real effect execution detours -> shared SDK source scope -> real query
    // adapter. No explicit scope in the normal path. All game objects remain
    // synthetic; this checks wiring/ABI/cleanup, not Unity scheduler coverage.
    {
        Behavior behavior_a{&ea},behavior_b{&eb},behavior_public{&ep};Plugin plugin_a{34,&ea};
        EffectScopes scopes(provider,refs,Fields,source,[&](const auto& text){logs.push_back(text);});
        in_effect=[&](int){Check(!TimelineCallScope::Active());};EffectVoid<0>(&behavior_a,&method_info);
        scopes.ActivateOnCurrentThread();
        in_effect=[&](int){Check(seen_effect_owner==&ea);
            Check(TimelineCallScope::Established() && TimelineCallScope::Current()==a);
            Check(Find(&manager,&query,nullptr,&method_info)==&ea);};
        EffectVoid<0>(&behavior_a,&method_info);EffectVoid<1>(&behavior_a,&method_info);
        EffectVoid<2>(&behavior_a,&method_info);EffectTick(&behavior_a,0.875f,&method_info);
        Check(seen_delta==0.875f);EffectVoid<4>(&plugin_a,&method_info);Check(handles.empty());
        in_effect=[&](int index) {
            if(index==1) {
                Check(TimelineCallScope::Current()==a);
                EffectVoid<0>(&behavior_public,&method_info);
                Check(TimelineCallScope::Current()==a);
                RaiseException(0xe0753015,0,0,nullptr);
            }
            Check(!TimelineCallScope::Current() && TimelineCallScope::Established());
            Check(Find(&manager,&query,nullptr,&method_info)==&ep);
        };
        Check(InvokeEffect(&behavior_a)==0xe0753015 && handles.empty() && !TimelineCallScope::Active());
        {TimelineCallScope outer(a);
            in_effect=[&](int){
                Check(seen_effect_owner==&eb);
                Check(TimelineCallScope::Current()==b);
                Check(Find(&manager,&query,nullptr,&method_info)==&eb);
            };
            EffectTick(&behavior_b,0.125f,&method_info);Check(TimelineCallScope::Current()==a);
            in_effect=[&](int){Check(TimelineCallScope::Active() && !TimelineCallScope::Established());
                Check(Find(&manager,&query,nullptr,&method_info)==&eb);};
            throw_owner=true;EffectVoid<0>(&behavior_a,&method_info);throw_owner=false;
            behavior_a.owner=nullptr;EffectVoid<0>(&behavior_a,&method_info);behavior_a.owner=&ea;
            ea.request=nullptr;EffectVoid<0>(&behavior_a,&method_info);ea.request=&ra;
            ra.caster=nullptr;EffectVoid<0>(&behavior_a,&method_info);ra.caster=&entity_a;
            change_generation=true;EffectVoid<0>(&behavior_a,&method_info);change_generation=false;
            Check(TimelineCallScope::Current()==a && handles.empty());
        }
        in_effect=[&](int){Check(!TimelineCallScope::Active());};
        std::thread worker([&]{EffectTick(&behavior_a,0.25f,&method_info);});worker.join();
        // Null caster may still have an explicit owned request receipt, but the
        // source callback (not the scope) must establish that ownership.
        in_effect=[&](int){Check(TimelineCallScope::Current()==a);};
        ra.caster=nullptr;owned_null_request=true;
        EffectVoid<1>(&behavior_a,&method_info);EffectVoid<4>(&plugin_a,&method_info);
        ra.caster=&entity_a;owned_null_request=false;Check(handles.empty());
        throw_release=true;EffectVoid<1>(&behavior_a,&method_info);throw_release=false;
        const auto frees=free_calls;scopes.Stop();Check(free_calls==frees && !handles.empty());
        handles.clear();in_effect=[&](int){Check(!TimelineCallScope::Active());};
        EffectVoid<0>(&behavior_a,&method_info);in_effect={};
    }
    {
        Plugin plugin_a{34,&ea},plugin_public{34,&ep};
        CameraController camera_a{&plugin_a},camera_public{&plugin_public};
        bool allow_camera=false,throw_policy=false;
        EffectScopes scopes(provider,refs,Fields,source,[&](const auto& text){logs.push_back(text);},
            [&](TimelineOwner owner){Check(owner==a);if(throw_policy)throw std::runtime_error("policy");return allow_camera;});
        scopes.ActivateOnCurrentThread();
        CameraEnable(&camera_a,true,&camera_info);CameraUpdate(&camera_a,&camera_update_info);
        Check(!camera_a.enabled && !camera_a.subscriptions && !camera_a.writes && handles.empty());
        allow_camera=true;CameraUpdate(&camera_a,&camera_update_info);
        Check(camera_a.enabled && camera_a.subscriptions==1 && camera_a.writes==1);
        allow_camera=false;CameraUpdate(&camera_a,&camera_update_info);
        Check(!camera_a.enabled && !camera_a.subscriptions && camera_a.writes==1);
        // Public controller nested inside owned caller uses its ACTUAL source.
        {TimelineCallScope outer(a);CameraUpdate(&camera_public,&camera_update_info);
            Check(camera_public.enabled && camera_public.writes==1 && TimelineCallScope::Current()==a);}
        // Target equality cannot make an official request owned.
        Check(rp.target==ra.target);
        CameraEnable(&camera_public,false,&camera_info);
        // Disabling is always passed through, even with a missing owner/getter.
        allow_camera=true;CameraEnable(&camera_a,true,&camera_info);allow_camera=false;
        const auto gets=camera_gets;camera_a.plugin=nullptr;
        CameraEnable(&camera_a,false,&camera_info);Check(camera_gets==gets && !camera_a.enabled);
        camera_a.plugin=&plugin_a;
        // Lookup or policy failure forwards/logs, never borrows an outer owner.
        {TimelineCallScope outer(a);throw_camera_getter=true;CameraUpdate(&camera_public,&camera_update_info);
            throw_camera_getter=false;Check(camera_public.writes==2 && TimelineCallScope::Current()==a);}
        CameraEnable(&camera_public,false,&camera_info);
        bad_source=true;CameraUpdate(&camera_a,&camera_update_info);bad_source=false;
        Check(camera_a.enabled && camera_a.writes==2);
        CameraEnable(&camera_a,false,&camera_info);
        throw_policy=true;CameraUpdate(&camera_a,&camera_update_info);throw_policy=false;
        Check(camera_a.enabled && camera_a.writes==3);
        // Original cleanup exception propagates; source scope/roots still close.
        throw_camera=true;Check(InvokeCamera(&camera_a)==0xe0753021);throw_camera=false;
        Check(handles.empty() && !TimelineCallScope::Active());
        CameraUpdate(&camera_a,&camera_update_info);Check(!camera_a.enabled && camera_a.writes==3);
        std::thread worker([&]{CameraUpdate(&camera_a,&camera_update_info);});worker.join();
        Check(camera_a.enabled && camera_a.writes==4);CameraEnable(&camera_a,false,&camera_info);
        const auto stopped_gets=camera_gets;scopes.Stop();CameraUpdate(&camera_a,&camera_update_info);
        Check(camera_a.enabled && camera_a.writes==5 && camera_gets==stopped_gets);
        CameraEnable(&camera_a,false,&camera_info);
        // An absent policy means observation-only, no camera source work.
        EffectScopes observe(provider,refs,Fields,source,{});observe.ActivateOnCurrentThread();
        CameraUpdate(&camera_a,&camera_update_info);Check(camera_a.writes==6 && camera_gets==stopped_gets);
        CameraEnable(&camera_a,false,&camera_info);Check(handles.empty());
    }
    {
        TimelineCallScope owned(a);
        throw_release=true;Check(Find(&manager,&query,nullptr,&method_info)==&ea);throw_release=false;
    }
    const auto released=free_calls;Check(!handles.empty());adapter.Stop();Check(free_calls==released);
    Check(Find(&manager,&query,nullptr,&method_info)==&eb); // detached; no destructor retry
    handles.clear(); // fixture-only cleanup, not production GC retry
    Check(!TimelineCallScope::Active());
    std::cout<<"Effect query/scopes candidate passed: real per-effect/plugin detours, original matcher/target/order, independent receiver/list, bidirectional sources, pool change, main-camera update+enable policy, official cleanup, nested/SEH cleanup and explicit fallback. Local fixtures; not game adoption.\n";
}
