#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <Windows.h>
#include <functional>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Params {
    unsigned id=5;bool released=false;void* effect=nullptr;
    Position3 offset{8,9,10};std::uint8_t present=0,pad[3]{};
    bool ignore=false,follow=true;float delay=9;bool replay=false;
} params;
struct Effect {void* request=nullptr;void* behavior=nullptr;float delay=9;void* native=&delay;bool replay=false;void* path=nullptr;} effect,foreign;
int manager,behavior,info,next=0,path,target;
EffectMutexKind mutex_kind=EffectMutexKind::None;void* candidate=nullptr;
std::map<int,void*> roots;
std::vector<std::string> calls;
std::function<void()> after_refresh,after_replay;
Position3 seen_direction;
void CheckAt(bool ok,int line){if(!ok)throw std::runtime_error("hit effect control test line "+std::to_string(line));}
#define Check(...) CheckAt((__VA_ARGS__),__LINE__)
template<class T>Method M(T p){return {reinterpret_cast<void*>(p),&info};}
int __fastcall Keep(void* object,int,int,void*){roots[++next]=object;return next;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1);}
unsigned __fastcall Id(void* p,void*){return static_cast<Params*>(p)->id;}
bool __fastcall Released(void* p,void*){return static_cast<Params*>(p)->released;}
void* __fastcall Instance(void* p,void*){return static_cast<Params*>(p)->effect;}
void __fastcall Refresh(void* m,void* p,void* e,void* meta){
    Check(m==&manager && p==&params && e==&effect && meta==&info);calls.push_back("refresh");if(after_refresh)after_refresh();
}
void __fastcall Replay(void* b,bool immediate,void* meta){
    Check(b==&behavior && !immediate && meta==&info);calls.push_back("replay");if(after_replay)after_replay();
}
void __fastcall Align(void* e,Position3 direction,void* meta){
    Check(e==&effect && meta==&info);calls.push_back("align");seen_direction=direction;
}
void __fastcall Revive(void* b,void* meta){Check(b==&behavior && meta==&info);calls.push_back("revive");}
EffectMutexKind __fastcall EntityMutex(void* m,void* p,float interval,int maximum,void* t,float delay,void* meta){
    Check(m==&manager && p==&path && t==&target && interval==.05f && maximum==1 && delay==.2f && meta==&info);return mutex_kind;
}
EffectMutexKind __fastcall GlobalMutex(void* m,void* p,float,int,void*){Check(m==&manager && p==&path);return mutex_kind;}
void* __fastcall First(void* m,void* p,void*,void*){Check(m==&manager && p==&path);return candidate;}
void* __fastcall Map(void* caster,void* p,void*){Check(caster==&target && p==&path);return p;}
bool __fastcall Empty(void*,void*){return false;}
bool __fastcall Equal(void* a,void* b,void*){return a==b;}
}
void TestHitEffectControl() {
    MetadataResolver resolver({
        {"","PGOOHIHKHNJ",{{"DMFKGEBBGGG","System.Void",{"RPG.Client.MonoEffect","RPG.MVector3"},true,M(Align)}}},
        {"System","String",{{"op_Equality","System.Boolean",{"System.String","System.String"},true,M(Equal)}}},
        {"RPG.Client","MonoEffectManager",{{"_TryGetAttachPointFromEntityAfterLoadEffect","System.Void",
            {"RPG.Client.TriggerEffectParams","RPG.Client.MonoEffect"},false,M(Refresh)},
            {"GetFirstActiveEffect","RPG.Client.MonoEffect",{"System.String","RPG.GameCore.GameEntity"},false,M(First)},
            {"GetFirstFadeOutEffect","RPG.Client.MonoEffect",{"System.String","RPG.GameCore.GameEntity"},false,M(First)}}},
        {"RPG.Client","MonoEffectBehavior",{{"Replay","System.Void",{"System.Boolean"},false,M(Replay)},
            {"Revive","System.Void",{},false,M(Revive)}}},
        {"RPG.GameCore","GamePlayStatic",{{"IsTriggerEffectMutexEntity","RPG.Client.EffectMutexResult",
            {"RPG.Client.MonoEffectManager","System.String","System.Single","System.Int32","RPG.GameCore.GameEntity","System.Single"},true,M(EntityMutex)},
            {"IsTriggerEffectMutexGlobal","RPG.Client.EffectMutexResult",{"RPG.Client.MonoEffectManager","System.String","System.Single","System.Int32"},true,M(GlobalMutex)}}}});
    std::vector<DeclaredField> pf{
        {"RPG.Client","TriggerEffectParams","PositionOffset","System.Nullable<UnityEngine.Vector3>",offsetof(Params,offset),6,true},
        {"RPG.Client","TriggerEffectParams","IgnorePrefabAttachPoint","System.Boolean",offsetof(Params,ignore),6,true},
        {"RPG.Client","TriggerEffectParams","FollowFirstFrameOnLateTick","System.Boolean",offsetof(Params,follow),6,true},
        {"RPG.Client","TriggerEffectParams","Delay","System.Single",offsetof(Params,delay),6,true},
        {"RPG.Client","TriggerEffectParams","IsEnableReplay","System.Boolean",offsetof(Params,replay),6,true}};
    std::vector<DeclaredField> ef{
        {"RPG.Client","MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request),6,true},
        {"RPG.Client","MonoEffect","_Behavior","RPG.Client.MonoEffectBehavior",offsetof(Effect,behavior),1,true},
        {"RPG.Client","MonoEffect","Delay","System.Single",offsetof(Effect,delay),6,true},
        {"RPG.Client","MonoEffect","EnableReplay","System.Boolean",offsetof(Effect,replay),6,true},
        {"UnityEngine","Object","m_CachedPtr","System.IntPtr",offsetof(Effect,native),1,true},
        {"RPG.Client","MonoEffect","<EffectName>k__BackingField","System.String",offsetof(Effect,path),1,true}};
    auto methods=ResolveHitEffectControl(resolver,pf,ef);
    Check(methods.fields_available && methods.position_offset==offsetof(Params,offset));
    auto bad=pf;bad[0].type="UnityEngine.Vector3";bool rejected=false;
    try{ResolveHitEffectControl(resolver,bad,ef);}catch(const std::exception&){rejected=true;}Check(rejected);
    EffectAccessMethods access;access.id=M(Id);access.released=M(Released);access.instance=M(Instance);
    access.map_path=M(Map);access.path_empty=M(Empty);
    HitEffectControl control(EffectAccess(access),methods,{M(Keep),M(Target),M(Free),{}, {}});
    const EffectReference ref{5,0,&params};
    HitEffectPlacement p{{1,2,3},{4,5,6},.2f,true};
    Check(control.Configure(&params,p).returned && params.offset.y==2 && params.present==1 && params.ignore && !params.follow && params.delay==.2f);
    // No loaded instance is an ordinary pending outcome; no replay/align.
    Check(control.Update(&manager,ref,p).state==EffectControlState::NotLoaded && calls.empty() && roots.empty());
    params.effect=&effect;effect.request=&params;effect.behavior=&behavior;
    Check(control.Loaded(ref,&foreign,p.direction).state==EffectControlState::Failed && calls.empty());
    Check(control.Loaded(ref,&effect,p.direction).state==EffectControlState::Applied && calls==std::vector<std::string>{"align"});
    Check(seen_direction.x==4 && seen_direction.y==5 && seen_direction.z==6 && roots.empty());
    calls.clear();params.effect=nullptr;
    Check(control.Loaded(ref,&effect,p.direction).state==EffectControlState::Applied); // callback precedes instance publication
    Check(calls==std::vector<std::string>{"align"});params.effect=&effect;
    calls.clear();params.delay=8;p.offset={7,8,9};p.delay=.4f;p.ignore_prefab_attach_point=false;
    Check(control.Update(&manager,ref,p).state==EffectControlState::Applied);
    Check(calls==std::vector<std::string>{"refresh","replay","align"});
    Check(params.offset.x==7 && !params.ignore && effect.delay==.4f && params.delay==8); // update changes MonoEffect delay only
    calls.clear();params.released=true;
    Check(control.Update(&manager,ref,p).state==EffectControlState::Ended && calls.empty());params.released=false;
    ++params.id;Check(control.Loaded(ref,&effect,p.direction).state==EffectControlState::Ended && calls.empty());params.id=5;
    effect.request=nullptr;Check(control.Update(&manager,ref,p).state==EffectControlState::Failed && calls.empty());effect.request=&params;
    after_refresh=[]{++params.id;};
    Check(control.Update(&manager,ref,p).state==EffectControlState::Ended && calls==std::vector<std::string>{"refresh"});
    after_refresh={};params.id=5;calls.clear();
    after_replay=[]{params.released=true;};
    Check(control.Update(&manager,ref,p).state==EffectControlState::Ended && calls==std::vector<std::string>{"refresh","replay"});
    after_replay={};params.released=false;calls.clear();
    after_refresh=[]{RaiseException(0xE0670001,0,0,nullptr);};
    const auto fault=control.Update(&manager,ref,p);
    Check(fault.state==EffectControlState::Failed && fault.call.exception==0xE0670001 && fault.references_released);
    Check(calls==std::vector<std::string>{"refresh"} && roots.empty());after_refresh={};
    calls.clear();mutex_kind=EffectMutexKind::Default;candidate=&foreign;
    auto decision=control.Mutex(&manager,nullptr,&target,&path,{.05f,1,EffectMutexScope::Entity},.2f,{ref});
    Check(decision.kind.read.returned && decision.kind.value==mutex_kind && !decision.replay && calls.empty());
    candidate=&effect;
    decision=control.Mutex(&manager,nullptr,&target,&path,{.05f,1,EffectMutexScope::Entity},.2f,{ref});
    Check(decision.replay && decision.replay->parameters==&params && calls.empty());
    Check(control.Replay(ref,mutex_kind).state==EffectControlState::NoMatch && calls.empty());
    params.replay=true;Check(control.Replay(ref,mutex_kind).state==EffectControlState::Applied);
    Check(calls==std::vector<std::string>{"revive","replay"});calls.clear();
    params.replay=false;Check(control.Replay(ref,EffectMutexKind::FadeOut).state==EffectControlState::Applied);
    Check(calls==std::vector<std::string>{"revive","replay"} && roots.empty());
    const auto no_match=control.Matches(ref,&target,&path);Check(no_match.read.returned && !no_match.value);
    effect.path=&path;const auto match=control.Matches(ref,&target,&path);Check(match.read.returned && match.value && roots.empty());
    Check(control.Close() && roots.empty() && !control.Configure(&params,p).returned);
}
