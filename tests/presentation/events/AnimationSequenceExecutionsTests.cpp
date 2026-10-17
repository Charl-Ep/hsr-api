#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

namespace {
using namespace hksr::api;
int info,actor,foreign_actor,world,manager,config,events,results,receiver;
unsigned generation=7;
struct Context {GameEntity* caster=reinterpret_cast<GameEntity*>(&actor);};
Context context,other_context{reinterpret_cast<GameEntity*>(&foreign_actor)};
struct Sequence {bool immediate=false;int state=0x7777;Context* context=&::context;};
Sequence persistent,immediate{true},nested,untracked;
struct Task {Context* context=&::context;void* config=&::config;};
Task task;
struct Request {GameEntity* caster=reinterpret_cast<GameEntity*>(&actor);unsigned id=1;};
Request request;
struct Handle {void* object;int kind;};
std::map<int,Handle> handles;int next_handle=0;
bool fail_weak=false,throw_original=false,child_sequence=false,child_untracked=false;
bool reenter_submit=false,finish_tick=false,check_error=false;
bool cleanup_effect=false,callback_effect=false,callback_throw=false;
unsigned begins=0,ticks=0,resets=0,disposes=0,immediate_begins=0,immediate_resets=0,immediate_disposes=0,submits=0;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* p,int,int kind,void*){if(fail_weak && kind==0)return 0;handles[++next_handle]={p,kind};return next_handle;}
void* __fastcall Target(int h,void*){return handles.at(h).object;}
void __fastcall Free(int h,void*){handles.erase(h);}
ReferenceMethods Refs(){return {M(Keep),M(Target),M(Free),{},{}};}
GameEntity* __fastcall Caster(Context* c,void*){return c->caster;}
__declspec(noinline) void __fastcall Trigger(Task*,void*);
__declspec(noinline) void __fastcall Async(void* m,Request* r,void* md) {
    Check(m==&manager && r==&request && md==&info,"effect arguments");++submits;
    if(reenter_submit){reenter_submit=false;++r->id;Trigger(&task,md);}
}
__declspec(noinline) void* __fastcall Sync(void* m,Request* r,void* md){Async(m,r,md);return &receiver;}
__declspec(noinline) void __fastcall Trigger(Task* t,void* md){Check(t==&task,"trigger argument");Async(&manager,&request,md);}
__declspec(noinline) void __fastcall Begin(Sequence* s,void* md) {
    ++begins;Check(md==&info,"begin metadata");
    if(check_error)Check(GetLastError()==1234,"begin input LastError");
    if(child_sequence){child_sequence=false;Begin(&nested,md);}
    if(child_untracked){child_untracked=false;Begin(&untracked,md);}
    Trigger(&task,md);
    if(throw_original)RaiseException(0xE078ABCD,0,0,nullptr);
    s->state=0x8888;if(check_error)SetLastError(5678);
}
__declspec(noinline) void __fastcall Tick(Sequence* s,float delta,void* md) {
    ++ticks;Check(delta==0.125f && md==&info,"tick ABI");
    Trigger(&task,md);s->state=finish_tick?0x9999:0x8888;
}
__declspec(noinline) void __fastcall Reset(Sequence* s,void* md){++resets;if(cleanup_effect)Trigger(&task,md);s->state=0x7777;}
__declspec(noinline) void __fastcall Dispose(Sequence* s,void* md){++disposes;if(cleanup_effect)Trigger(&task,md);s->state=0x7777;}
__declspec(noinline) void __fastcall ImmediateBegin(Sequence* s,void* md){++immediate_begins;Trigger(&task,md);s->state=0x9999;}
__declspec(noinline) void __fastcall ImmediateReset(Sequence* s,void*){++immediate_resets;s->state=0x7777;}
__declspec(noinline) void __fastcall ImmediateDispose(Sequence* s,void*){++immediate_disposes;s->state=0x7777;}
bool TryBegin(){__try{Begin(&persistent,&info);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
__declspec(noinline) void* __fastcall Build(Context* c,void* configs,void* parent,void* md) {
    Check(c==&context && configs==&config && parent && md==&info,"builder arguments");return &results;
}
__declspec(noinline) void __fastcall Construct(void* self,void* e,void*,void*,void*,void* c,void*,void*,void* md) {
    Check(self==&receiver && e==&events,"construction arguments");Build(static_cast<Context*>(c),&config,&persistent,md);
}
std::vector<DeclaredField> Fields(const char* ns,const char* owner) {
    const std::string name=owner;
    if(name=="TriggerEffectParams")return {
        {ns,owner,"CasterEntity","RPG.GameCore.GameEntity",offsetof(Request,caster),6,true},
        {ns,owner,"<UniqueEffectID>k__BackingField","System.UInt32",offsetof(Request,id),6,true}};
    if(name=="NFGENDDOCJJ")return {
        {ns,owner,"OFKGLJOAMLD","RPG.GameCore.TriggerEffect",offsetof(Task,config),6,true},
        {ns,owner,"DDDNLILNMKA","RPG.GameCore.TaskContext",offsetof(Task,context),6,true}};
    return {{ns,owner,name=="KFELGAIGHAP"?"<TaskContext>k__BackingField":"DDDNLILNMKA","RPG.GameCore.TaskContext",offsetof(Sequence,context),1,true},
        {"","GELIALJDFBL","DELMCJFBNBL","RPG.GameCore.TaskState",offsetof(Sequence,state),4,true}};
}
EntityOrigin Identify(GameEntity* e){return e==reinterpret_cast<GameEntity*>(&actor)?EntityOrigin{{e,generation},reinterpret_cast<GameWorld*>(&world)}:EntityOrigin{};}
struct Types:ObjectMetadata {
    DeclaredType ReadObjectType(void*)override{throw std::logic_error("no full type enumeration needed");}
    std::vector<DeclaredField> ReadObjectFields(void*)override{return {};}
    RuntimeTypeName ReadObjectTypeName(void* object)override{return {"",static_cast<Sequence*>(object)->immediate?"MKDHBHBFEDD":"KFELGAIGHAP"};}
};
AnimationEventSource Source(){AnimationEventSource s;s.serial=21;s.origin=Identify(context.caster);return s;}
AnimationTaskConstruction Definition(Sequence& sequence,std::uint64_t id=42){return {id,sequence.context,&config,&sequence,&results,true};}
void OnlyWeak(){Check(std::all_of(handles.begin(),handles.end(),[](const auto& h){return h.second.kind==0;}),"strong roots persisted after call");}
}
int main() {
    try {
        MetadataResolver methods({
            {"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}},
            {"","KFELGAIGHAP",{
                {"OnTaskBegin","System.Void",{},false,M(Begin)},{"Tick","System.Void",{"System.Single"},false,M(Tick)},
                {"OnTaskReset","System.Void",{},false,M(Reset)},{"Dispose","System.Void",{},false,M(Dispose)}}},
            {"","MKDHBHBFEDD",{
                {"OnTaskBegin","System.Void",{},false,M(ImmediateBegin)},
                {"OnTaskReset","System.Void",{},false,M(ImmediateReset)},{"Dispose","System.Void",{},false,M(ImmediateDispose)}}},
            {"","NFGENDDOCJJ",{{"OnTaskBegin","System.Void",{},false,M(Trigger)}}},
            {"RPG.Client","MonoEffectManager",{
                {"FireEffectAsync","System.Void",{"RPG.Client.TriggerEffectParams"},false,M(Async)},
                {"FireEffect","RPG.Client.MonoEffect",{"RPG.Client.TriggerEffectParams"},false,M(Sync)}}},
            {"","JKNLHJLPIOO",{{"KJCNEJALIJO","GELIALJDFBL[]",{"RPG.GameCore.TaskContext","System.Collections.Generic.IEnumerable<RPG.GameCore.TaskConfig>","BNBOAMEBAOE"},true,M(Build)}}},
            {"","HHECHLOIEJO",{{"FGAHFIBMPCP","System.Void",{"RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]","RPG.GameCore.AnimGroupEventConfig[]","RPG.GameCore.LogicAnimEventEntry[]","RPG.GameCore.TaskContext","LDMKDJJLOIL","System.String"},false,M(Construct)}}}
        });
        Types types;std::vector<AnimationSequenceObservation> observations;std::vector<EffectSubmission> effects;std::vector<std::string> logs;
        AnimationSequenceExecutions* stop_during_begin=nullptr;
        AnimationSequenceExecutions sequences(methods,types,Refs(),Fields,Identify,[&](const auto& o){
            observations.push_back(o);
            if(callback_effect){callback_effect=false;Trigger(&task,&info);}
            if(stop_during_begin && o.entering){auto* p=stop_during_begin;stop_during_begin=nullptr;p->Close();}
            if(check_error)SetLastError(9000);
            if(callback_throw)throw std::runtime_error("consumer");
        },[&](const auto& s){logs.push_back(s);});
        EffectSubmissions submissions(methods,Refs(),Fields,Identify,[&](const auto& e){effects.push_back(e);},[](const auto&,bool){});
        sequences.ActivateOnCurrentThread();submissions.ActivateOnCurrentThread();
        std::uint64_t construction=0,definition=0;
        AnimationEventConstruction sources(methods,Refs(),Identify,[](const auto&){},[&](const auto& s,const auto& b){
            construction=s.serial;definition=b.serial;
            Check(sequences.Track(s,b)==AnimationSequenceTracking::Tracked,"actual construction not tracked");
        },[](const auto&,bool){});sources.ActivateOnCurrentThread();
        Construct(&receiver,&events,nullptr,nullptr,nullptr,&context,nullptr,nullptr,&info);
        OnlyWeak();Check(handles.size()==1,"definition not weakly tracked");
        check_error=true;SetLastError(1234);Begin(&persistent,&info);check_error=false;
        Check(GetLastError()==5678,"original output LastError changed");
        auto first=effects.back().sequence;
        Check(first.serial && first.definition==definition && first.construction==construction && first.sequence==&persistent,"construction-to-effect linkage");
        Check(effects.back().trigger.serial && observations.back().task_state==0x8888 && observations.back().continues,"task linkage/state");
        Tick(&persistent,.125f,&info);Check(effects.back().sequence.serial==first.serial,"continuation got new execution");
        finish_tick=true;Tick(&persistent,.125f,&info);finish_tick=false;
        Check(effects.back().sequence.serial==first.serial && !observations.back().continues,"terminal tick lost execution or stayed active");
        Tick(&persistent,.125f,&info);Check(!effects.back().sequence.serial,"tick guessed missing Begin");
        Begin(&persistent,&info);Check(effects.back().sequence.serial!=first.serial,"replay retained old identity");
        first=effects.back().sequence;Reset(&persistent,&info);Tick(&persistent,.125f,&info);
        Check(!effects.back().sequence.serial,"reset left active execution");
        Begin(&persistent,&info);Check(effects.back().sequence.serial!=first.serial,"reset replay identity");
        sequences.Track(Source(),Definition(nested,43));child_sequence=true;auto n=effects.size();Begin(&persistent,&info);
        Check(effects.size()==n+2 && effects[n].sequence.serial && effects[n].sequence.serial!=effects[n+1].sequence.serial,"nested execution identity");
        Check(effects[n].sequence.parent_serial==effects[n+1].sequence.serial,"nested parent causality");
        child_untracked=true;n=effects.size();Begin(&persistent,&info);
        Check(!effects[n].sequence.serial && effects[n+1].sequence.serial,"untracked nested sequence inherited parent");
        callback_effect=true;n=effects.size();Begin(&persistent,&info);
        Check(!effects[n].sequence.serial && effects[n+1].sequence.serial,"observer reentry inherited sequence");
        reenter_submit=true;n=effects.size();Begin(&persistent,&info);
        Check(effects.size()==n+2 && effects[n].sequence.serial && !effects[n+1].sequence.serial,
            "submission plugin inherited the outer sequence");
        Trigger(&task,&info);Check(!effects.back().sequence.serial,"scope escaped producing stack");
        throw_original=true;Check(!TryBegin() && !observations.back().original_returned,"exception changed");throw_original=false;
        Tick(&persistent,.125f,&info);Check(!effects.back().sequence.serial,"exception left running execution");
        callback_throw=true;const auto b=begins;Begin(&persistent,&info);callback_throw=false;
        Check(begins==b+1 && !logs.empty(),"consumer prevented original");
        sequences.Track(Source(),Definition(immediate,44));ImmediateBegin(&immediate,&info);auto instant=effects.back().sequence.serial;
        Check(instant && effects.back().sequence.kind==AnimationSequenceKind::Immediate && !observations.back().continues,"immediate execution absent or stayed active");
        ImmediateBegin(&immediate,&info);Check(effects.back().sequence.serial!=instant,"immediate replay conflated");
        ImmediateReset(&immediate,&info);ImmediateDispose(&immediate,&info);
        cleanup_effect=true;Reset(&persistent,&info);Check(!effects.back().sequence.serial,"reset effect attributed to playback");
        Dispose(&persistent,&info);cleanup_effect=false;Begin(&persistent,&info);Check(!effects.back().sequence.serial,"disposed definition reused");
        sequences.Collect();OnlyWeak();
        sequences.Track(Source(),Definition(persistent));++generation;Begin(&persistent,&info);
        Check(!effects.back().sequence.serial,"entity generation reuse inherited execution");--generation;
        sequences.Track(Source(),Definition(persistent));
        persistent.context=&other_context;Begin(&persistent,&info);Check(!effects.back().sequence.serial,"foreign context attributed");persistent.context=&context;
        sequences.Track(Source(),Definition(persistent));
        const auto count=observations.size();std::thread other([]{Begin(&persistent,&info);});other.join();Check(observations.size()==count,"foreign thread observed");
        // Simulated GC followed by address reuse cannot revive the old receipt.
        Begin(&persistent,&info);const auto collected_execution=effects.back().sequence.serial;
        Check(collected_execution!=0,"collection test requires a running execution");
        for(auto& h:handles)if(h.second.object==&persistent && h.second.kind==0)h.second.object=nullptr;
        sequences.Collect();
        Check(observations.back().operation==AnimationSequenceOperation::DefinitionRetired &&
            observations.back().execution.serial==collected_execution && !observations.back().continues &&
            !observations.back().original_returned && !observations.back().state_read,"collection did not retire exact producer identity");
        Begin(&persistent,&info);Check(!effects.back().sequence.serial,"collected identity revived by address reuse");
        sequences.Collect();fail_weak=true;Check(sequences.Track(Source(),Definition(persistent))==AnimationSequenceTracking::Unavailable,"weak failure accepted");fail_weak=false;
        sequences.Collect();
        std::vector<AnimationSequenceObservation> secondary;
        AnimationSequenceExecutions second(methods,types,Refs(),Fields,Identify,[&](const auto& o){secondary.push_back(o);});second.ActivateOnCurrentThread();
        sequences.Track(Source(),Definition(persistent));second.Track(Source(),Definition(persistent));
        Begin(&persistent,&info);Check(secondary.back().execution.serial==observations.back().execution.serial,"subscribers disagree on invocation ID");
        sequences.Stop();Begin(&persistent,&info);Check(effects.back().sequence.serial==secondary.back().execution.serial,"stop evicted another observer");
        Check(sequences.Close() && second.Close(),"close failed");sources.Stop();Check(handles.empty(),"roots or weak handles leaked");
        // Closing in a callback releases definitions without breaking the active
        // native call; no later effects inherit the stopped source.
        AnimationSequenceExecutions last(methods,types,Refs(),Fields,Identify,[&](const auto& o){if(o.entering)stop_during_begin->Close();});
        last.ActivateOnCurrentThread();last.Track(Source(),Definition(persistent));stop_during_begin=&last;Begin(&persistent,&info);
        Check(!effects.back().sequence.serial && last.Close() && handles.empty(),"in-flight close retained provenance or handles");submissions.Stop();
        std::cout<<"Construction, replay, continuation, nested scope, cleanup, effects, weak identity and subscriber tests passed.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
