#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <Windows.h>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool value){if(!value)throw std::runtime_error("effect entity follow test failed");}
struct List {std::vector<void*> items;};
struct Plugin;
struct Behavior {List* targets=nullptr;Plugin* owner=nullptr;};
struct Effect;
struct Plugin {Effect* owner=nullptr;Behavior* behavior=nullptr;};
struct Request {unsigned id=0;bool released=false;Effect* effect=nullptr;};
struct Effect {Request* request=nullptr;Plugin* plugin=nullptr;bool alive=true;};
List one_list,two_list,input;
Behavior one_behavior{&one_list},two_behavior{&two_list};
Plugin one_plugin{nullptr,&one_behavior},two_plugin{nullptr,&two_behavior};
Effect one{nullptr,&one_plugin},two{nullptr,&two_plugin};
Request first{10,false,&one},second{20,false,&two};
int info,plugin_type,entity_type,target1,target2;
int movement_type;
struct Movement {void* owner;bool attached;};
Movement movement{&target1,false};
int serial=0,binds=0,clears=0,allocations=0;
bool absent=false,foreign_behavior=false,fault=false,no_bind=false;
std::map<int,void*> roots;
std::function<void()> on_component,on_array;
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
bool Kept(void* value){for(const auto& [id,item]:roots){(void)id;if(item==value)return true;}return false;}
int __fastcall Retain(void* value,int,int,void*){roots[++serial]=value;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1);}
unsigned __fastcall Id(void* value,void*){return static_cast<Request*>(value)->id;}
bool __fastcall Released(void* value,void*){return static_cast<Request*>(value)->released;}
void* __fastcall Instance(void* value,void*){return static_cast<Request*>(value)->effect;}
bool __fastcall Alive(void* value,void*){return value==&one?one.alive:value==&two?two.alive:true;}
void* __fastcall Component(void* value,void* type,void*) {
    Check(Kept(value) && Kept(type) && type==&plugin_type);
    if(on_component)on_component();
    return absent?nullptr:static_cast<Effect*>(value)->plugin;
}
void* __fastcall GetBehavior(void* value,void*){Check(Kept(value));return static_cast<Plugin*>(value)->behavior;}
void* __fastcall GetOwner(void* value,void*){Check(Kept(value));return foreign_behavior?&two_plugin:static_cast<Behavior*>(value)->owner;}
void* __fastcall MovementComponent(void* value,void* type,void*) {
    Check(Kept(value) && Kept(type) && type==&movement_type);return absent?nullptr:&movement;
}
int __fastcall Count(void* value,void*){Check(Kept(value));return static_cast<int>(static_cast<List*>(value)->items.size());}
void* __fastcall Item(void* value,int index,void*){Check(Kept(value));return static_cast<List*>(value)->items.at(index);}
void* __fastcall CreateArray(void* type,int count,void*) {
    Check(type==&entity_type && Kept(type));++allocations;input.items.assign(count,nullptr);
    if(on_array)on_array();return &input;
}
void __fastcall SetArray(void* value,void* item,int index,void*) {
    Check(value==&input && Kept(value) && Kept(item));input.items.at(index)=item;
}
void __fastcall Bind(void* value,void* entities,void* meta) {
    Check(Kept(value) && Kept(entities) && meta==&info);++binds;
    if(!no_bind)static_cast<Behavior*>(value)->targets->items=static_cast<List*>(entities)->items;
    if(fault)RaiseException(0xE0670002,0,0,nullptr);
}
void __fastcall Clear(void* value,void* meta) {
    Check(Kept(value) && meta==&info);++clears;static_cast<Behavior*>(value)->targets->items.clear();
}
struct Objects:ManagedObjects {
    ObjectResult NewStringUtf8(const char*)override{throw std::runtime_error("unexpected string allocation");}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("unexpected task allocation");}
    ObjectResult ManagedType(const char* ns,const char* type)override {
        if(std::string(type)=="BGOBCKLPEMK"){Check(std::string(ns).empty());return {{true,0},&movement_type};}
        const bool plugin=std::string(type)=="MonoEffectPluginEntityFollowEffect";
        Check(plugin?std::string(ns)=="RPG.Client":std::string(ns)=="RPG.GameCore" && std::string(type)=="GameEntity");
        return {{true,0},plugin?&plugin_type:&entity_type};
    }
};
}
void TestEffectEntityFollow() {
    one.request=&first;two.request=&second;one_plugin.owner=&one;two_plugin.owner=&two;
    one_behavior.owner=&one_plugin;two_behavior.owner=&two_plugin;
    MetadataResolver metadata({
        {"RPG.GameCore","GameEntity",{{"GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"},false,M(MovementComponent)}}},
        {"UnityEngine","Component",{{"GetComponent","UnityEngine.Component",{"System.Type"},false,M(Component)}}},
        {"UnityEngine","Object",{{"op_Implicit","System.Boolean",{"UnityEngine.Object"},true,M(Alive)}}},
        {"RPG.Client","MonoEffectPluginEntityFollowEffect",{{"get_Behavior","GIMGIMFGBFF",{},false,M(GetBehavior)}}},
        {"","GIMGIMFGBFF",{{"HPNDIIPEFLJ","RPG.Client.MonoEffectPluginEntityFollowEffect",{},false,M(GetOwner)},
            {"CDEGKEKEHID","System.Void",{"System.Collections.Generic.IList<RPG.GameCore.GameEntity>"},false,M(Bind)},
            {"CHBEGMHKJKP","System.Void",{},false,M(Clear)}}}});
    auto methods=ResolveEffectEntityFollow460(metadata);Check(methods.binding_errors.empty());
    const auto fields=ResolveEffectEntityFollowFields460(
        {{"RPG.Client","MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request),6,true}},
        {{"RPG.Client","MonoEffectPluginBase","_OwnerEffect","RPG.Client.MonoEffect",offsetof(Plugin,owner),4,true}},
        {{"","GIMGIMFGBFF","PJFPEKPDKIH","System.Collections.Generic.List<RPG.GameCore.GameEntity>",offsetof(Behavior,targets),33,true}});
    EffectAccessMethods am;am.id=M(Id);am.released=M(Released);am.instance=M(Instance);
    ArrayMethods ar;ar.create=M(CreateArray);ar.set=M(SetArray);
    ReferenceListMethods lm;lm.count=M(Count);lm.item=M(Item);
    const ReferenceMethods refs{M(Retain),M(Target),M(Free),{}, {}};
    Objects objects;
    const auto target_binding=ResolveEffectMotionTarget460(metadata,{
        {"RPG.GameCore","GameComponentBase","_OwnerRef","RPG.GameCore.GameEntity",offsetof(Movement,owner),6,true},
        {"","BGOBCKLPEMK","<IsAttachMotionWithEffect>k__BackingField","System.Boolean",offsetof(Movement,attached),1,true}});
    EffectEntityFollow follow(EffectAccess(am),methods,fields,refs,objects,ManagedArrays(ar),ManagedReferenceList(lm),target_binding);
    Check(follow.ReadTarget(&target1).result.state==EffectControlState::Applied && !follow.ReadTarget(&target1).attached);
    movement.attached=true;Check(follow.ReadTarget(&target1).attached);movement.attached=false;
    Check(follow.ReadTarget(&target2).result.state==EffectControlState::Failed);
    absent=true;Check(follow.ReadTarget(&target1).result.state==EffectControlState::Unsupported);absent=false;
    const EffectReference a{10,0,&first},b{20,0,&second};
    Check(follow.Read(a).result.state==EffectControlState::Applied && allocations==0);
    Check(follow.Set(a,{}, {&target1,&target2}).result.state==EffectControlState::Applied);
    Check(follow.Read(a).targets==std::vector<void*>({&target1,&target2}));
    Check(follow.Set(b,{}, {&target2}).result.state==EffectControlState::Applied);
    const auto calls=binds;
    const auto prior_allocations=allocations;
    Check(follow.Set(a,{&target1,&target2},{&target1,&target2}).result.state==EffectControlState::Applied &&
        binds==calls && allocations==prior_allocations);
    Check(follow.Set(a,{}, {&target2}).result.state==EffectControlState::Failed && binds==calls);
    Check(follow.Set(a,{&target1,&target2},{}).result.state==EffectControlState::Applied && clears==1);
    Check(follow.Read(b).targets==std::vector<void*>({&target2}));
    Check(follow.Set(a,{}, {&target1,&target1}).result.state==EffectControlState::Failed && binds==calls);
    Check(follow.Set(a,{}, {nullptr}).result.state==EffectControlState::Failed && binds==calls);
    first.effect=nullptr;Check(follow.Set(a,{}, {&target1}).result.state==EffectControlState::NotLoaded);first.effect=&one;
    first.released=true;Check(follow.Read(a).result.state==EffectControlState::Ended);first.released=false;
    ++first.id;Check(follow.Read(a).result.state==EffectControlState::Ended);first.id=10;
    one.request=&second;Check(follow.Read(a).result.state==EffectControlState::Failed);one.request=&first;
    one_plugin.owner=&two;Check(follow.Read(a).result.state==EffectControlState::Failed);one_plugin.owner=&one;
    foreign_behavior=true;Check(follow.Read(a).result.state==EffectControlState::Failed);foreign_behavior=false;
    absent=true;Check(follow.Read(a).result.state==EffectControlState::Unsupported);absent=false;
    one.alive=false;Check(follow.Read(a).result.state==EffectControlState::Failed);one.alive=true;
    on_component=[] {++first.id;};Check(follow.Set(a,{}, {&target1}).result.state==EffectControlState::Ended);on_component={};first.id=10;
    on_array=[] {one_list.items={&target2};};
    Check(follow.Set(a,{}, {&target1}).result.state==EffectControlState::Failed && binds==calls);
    on_array={};one_list.items.clear();
    on_array=[] {one_behavior.owner=&two_plugin;};
    Check(follow.Set(a,{}, {&target1}).result.state==EffectControlState::Failed && binds==calls);
    on_array={};one_behavior.owner=&one_plugin;
    bool current=true;
    on_array=[&]{current=false;};
    Check(follow.Set(a,{}, {&target1},[&]{return current;}).result.state==EffectControlState::Failed && binds==calls);
    on_array={};
    no_bind=true;Check(follow.Set(a,{}, {&target1}).result.state==EffectControlState::Failed);no_bind=false;
    fault=true;const auto failed=follow.Set(a,{}, {&target1});fault=false;
    Check(failed.result.state==EffectControlState::Failed && failed.result.call.exception==0xE0670002 && failed.result.references_released);
    Check(follow.Read(a).targets==std::vector<void*>({&target1})); // partial writes retained as facts
    Check(follow.Set(a,{&target1},{}).result.state==EffectControlState::Applied);
    methods.bind={};
    EffectEntityFollow read_only(EffectAccess(am),methods,fields,refs,objects,ManagedArrays(ar),ManagedReferenceList(lm));
    Check(read_only.Read(a).result.state==EffectControlState::Applied);
    Check(read_only.Set(a,{}, {&target1}).result.state==EffectControlState::Unsupported);
    Check(read_only.Set(b,{&target2},{}).result.state==EffectControlState::Applied);
    Check(read_only.Close() && follow.Close() && roots.empty());
    std::cout<<"Effect entity follow: exact requests, target readback, rooted array ABI, ownership conflicts and partial writes passed; no game motion acceptance.\n";
}
