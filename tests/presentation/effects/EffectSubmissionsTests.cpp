#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
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
int info,manager,world,actor,public_actor,config,instance,callback_marker;
struct Request {
    int padding[7]{};
    GameEntity* caster=reinterpret_cast<GameEntity*>(&actor);
    unsigned id=0;bool released=false;
    void* callback=&callback_marker;
    void* target=&actor;
};
Request request,other;
struct Context {GameEntity* caster=reinterpret_cast<GameEntity*>(&actor);};
struct Task {int pad[11]{};Context* context=nullptr;void* config=nullptr;};
Context context;Task task;
std::map<int,void*> roots;int next_root=0;
void* fail_keep=nullptr;
unsigned async_calls=0,sync_calls=0,trigger_calls=0,completions=0,removes=0;
bool forward_sync=false,preprocess_failure=false,original_fault=false,submit_child=false,skip_submit=false;
bool nest_trigger=false,recycle_id=false;
bool check_last_error=false;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* object,int,int,void*){if(object==fail_keep)return 0;roots[++next_root]=object;return next_root;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
ReferenceMethods Refs(){return {M(Keep),M(Target),M(Free),{},{}};}
GameEntity* __fastcall Caster(Context* c,void*){return c->caster;}
unsigned __fastcall Id(Request* r,void*){return r->id;}
bool __fastcall Released(Request* r,void*){return r->released;}
int __fastcall Status(Request*,void*){return 0;}
void* __fastcall Instance(Request*,void*){return nullptr;}
bool __fastcall Remove(void*,EffectReference ref,bool,void*){++removes;static_cast<Request*>(ref.parameters)->released=true;return true;}
__declspec(noinline) void* __fastcall Sync(void* receiver,Request* r,void* metadata) {
    if(check_last_error)Check(GetLastError()==1234,"original LastError input changed");
    Check(receiver==&manager && metadata==&info,"sync ABI");++sync_calls;
    Check(r->callback==&callback_marker,"official callback overwritten");
    if(original_fault)RaiseException(0xE0683451,0,0,nullptr);
    if(preprocess_failure)return nullptr;
    ++completions;
    if(recycle_id){const bool saved=recycle_id;recycle_id=false;++r->id;Sync(receiver,r,metadata);recycle_id=saved;}
    if(submit_child){submit_child=false;Sync(receiver,&other,metadata);}
    if(check_last_error)SetLastError(5678);
    return &instance;
}
__declspec(noinline) void __fastcall Async(void* receiver,Request* r,void* metadata) {
    Check(receiver==&manager && metadata==&info,"async ABI");++async_calls;
    if(forward_sync)Sync(receiver,r,metadata);
    else if(original_fault)RaiseException(0xE0683452,0,0,nullptr);
}
__declspec(noinline) void __fastcall Trigger(Task* value,void* metadata) {
    Check(value==&task && metadata==&info,"task ABI");++trigger_calls;
    if(nest_trigger){nest_trigger=false;Trigger(value,metadata);}
    if(!skip_submit)Async(&manager,&request,metadata);
}
bool TryAsync(){__try{Async(&manager,&request,&info);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool TryTrigger(){__try{Trigger(&task,&info);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
std::vector<DeclaredField> Fields(const char* ns,const char* type) {
    if(std::string(type)=="TriggerEffectParams")return {
        {ns,type,"CasterEntity","RPG.GameCore.GameEntity",offsetof(Request,caster),6,true},
        {ns,type,"<UniqueEffectID>k__BackingField","System.UInt32",offsetof(Request,id),6,true}};
    return {{ns,type,"OFKGLJOAMLD","RPG.GameCore.TriggerEffect",offsetof(Task,config),6,true},
        {ns,type,"DDDNLILNMKA","RPG.GameCore.TaskContext",offsetof(Task,context),6,true}};
}
EntityOrigin Identify(GameEntity* e) {
    if(e!=reinterpret_cast<GameEntity*>(&actor))return {};
    return {{e,7},reinterpret_cast<GameWorld*>(&world)};
}
EffectRequestServices Services() {
    EffectAccessMethods methods;methods.id=M(Id);methods.released=M(Released);
    methods.state=M(Status);methods.instance=M(Instance);methods.remove=M(Remove);
    return {Refs(),EffectAccess(methods),{},[]{return ObjectResult{{true,0},&manager};},{}};
}
}
int main() {
    try {
        task.context=&context;task.config=&config;other.id=23;
        MetadataResolver metadata({
            {"RPG.Client","MonoEffectManager",{
                {"FireEffectAsync","System.Void",{"RPG.Client.TriggerEffectParams"},false,M(Async)},
                {"FireEffect","RPG.Client.MonoEffect",{"RPG.Client.TriggerEffectParams"},false,M(Sync)}}},
            {"","NFGENDDOCJJ",{{"OnTaskBegin","System.Void",{},false,M(Trigger)}}},
            {"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}}
        });
        std::vector<EffectSubmission> begins,ends;std::vector<bool> returned;std::vector<std::string> logs;
        std::unique_ptr<OwnedEffectRequest> owned;
        bool retain=false,callback_fault=false,callback_reentry=false;
        EffectSubmissions observer(metadata,Refs(),Fields,Identify,[&](const auto& submission){
            begins.push_back(submission);
            Check(std::any_of(roots.begin(),roots.end(),[&](auto p){return p.second==submission.manager;}),"manager not retained before callback");
            Check(submission.origin.token.generation==7,"origin generation");
            if(retain){owned=std::make_unique<OwnedEffectRequest>(Services());Check(owned->TrackSubmitted(submission.manager,submission.request),"track receipt");}
            if(callback_reentry){callback_reentry=false;Sync(&manager,&other,&info);}
            if(callback_fault)throw std::runtime_error("test consumer fault");
            if(check_last_error)SetLastError(9876);
        },[&](const auto& submission,bool success){ends.push_back(submission);returned.push_back(success);if(check_last_error)SetLastError(8765);},
        [&](const auto& line){logs.push_back(line);});
        Async(&manager,&request,&info);Check(begins.empty(),"inactive observer");
        observer.ActivateOnCurrentThread();forward_sync=true;
        check_last_error=true;SetLastError(1234);Sync(&manager,&request,&info);
        Check(GetLastError()==5678,"original LastError result changed");check_last_error=false;begins.clear();ends.clear();returned.clear();
        Trigger(&task,&info);
        Check(begins.size()==1 && ends.size()==1 && returned.back(),"nested forwarding duplicate");
        const auto first=begins.back();
        Check(first.request.id==0 && first.trigger.serial && first.trigger.task==&task && first.trigger.config==&config && first.trigger.context==&context,"task invocation facts");
        Check(roots.empty(),"invocation roots leaked");
        Trigger(&task,&info);
        Check(begins.back().trigger.serial!=first.trigger.serial && begins.back().serial!=first.serial,"reused task conflates executions");
        skip_submit=true;const auto size=begins.size();Trigger(&task,&info);skip_submit=false;
        Check(begins.size()==size,"mutex/no submission falsely adopted");
        nest_trigger=true;Trigger(&task,&info);
        Check(begins[begins.size()-2].trigger.serial!=begins.back().trigger.serial,"nested task execution conflated");
        // A callback/plugin inside original submission is not the outer task.
        submit_child=true;Trigger(&task,&info);
        Check(begins.back().request.parameters==&other && !begins.back().trigger.serial,"plugin inherited enclosing task");
        callback_reentry=true;Trigger(&task,&info);
        Check(!begins.back().trigger.serial,"observer reentry inherited enclosing task");
        Check(Sync(&manager,&request,&info)==&instance,"sync result changed");
        Check(begins.back().kind==EffectSubmissionKind::Synchronous && !begins.back().trigger.serial,"direct source invented");
        preprocess_failure=true;const auto before_failure=begins.size();
        Check(Sync(&manager,&request,&info)==nullptr,"null sync result changed");
        Check(begins.size()==before_failure+1 && returned.back(),"preprocessing failure lost receipt");preprocess_failure=false;
        original_fault=true;Check(!TryAsync() && !returned.back(),"original exception swallowed");original_fault=false;
        original_fault=true;Check(!TryTrigger() && !returned.back(),"task exception swallowed");original_fault=false;
        Sync(&manager,&request,&info);Check(!begins.back().trigger.serial,"exception left task scope active");
        callback_fault=true;const auto original_before=sync_calls;Sync(&manager,&request,&info);callback_fault=false;
        Check(sync_calls==original_before+1 && returned.back() && !logs.empty(),"consumer fault stopped original");
        // Request address reuse while nested is a different generation.
        recycle_id=true;const auto reused=begins.size();Sync(&manager,&request,&info);recycle_id=false;
        Check(begins.size()==reused+2 && begins.back().request.id==1,"nested recycled request discarded");
        const auto filtered=begins.size();request.caster=nullptr;Sync(&manager,&request,&info);
        request.caster=reinterpret_cast<GameEntity*>(&public_actor);Sync(&manager,&request,&info);
        Check(begins.size()==filtered,"target treated as source");request.caster=reinterpret_cast<GameEntity*>(&actor);
        context.caster=reinterpret_cast<GameEntity*>(&public_actor);Trigger(&task,&info);
        Check(!begins.back().trigger.serial,"different task caster adopted");context.caster=request.caster;
        const auto failed_keep=begins.size();fail_keep=&request;Sync(&manager,&request,&info);fail_keep=nullptr;
        Check(begins.size()==failed_keep && roots.empty(),"failed retention receipt exposed");
        // Original completion remains intact; consumer owns receipt before it.
        retain=true;const auto completed=completions;Trigger(&task,&info);retain=false;
        Check(owned && owned->Status().submitted && completions==completed+1 && request.callback==&callback_marker,"original completion lost");
        owned->Cancel();Check(!removes,"cancellation ran on producer stack");owned->Service();
        Check(removes==1 && owned->Status().closed && roots.empty(),"tracked receipt retirement");owned.reset();request.released=false;
        // Independent subscriptions share one hook; stopping one leaves others.
        unsigned second_before=0,second_after=0;
        EffectSubmissions second(metadata,Refs(),Fields,Identify,[&](const auto&){++second_before;},
            [&](const auto&,bool){++second_after;});second.ActivateOnCurrentThread();
        Sync(&manager,&request,&info);Check(second_before==1 && second_after==1,"second subscriber absent");
        observer.Stop();const auto stopped=begins.size();Sync(&manager,&request,&info);
        Check(begins.size()==stopped && second_before==2 && second_after==2,"stop evicted other subscriber");
        std::thread foreign([]{Sync(&manager,&request,&info);});foreign.join();Check(second_before==2,"foreign thread observed");
        second.Stop();Check(roots.empty(),"remaining roots");
        // Re-subscribe after all stopped; no reinstall collision. Stop during
        // Before must detach After without interrupting the original call.
        EffectSubmissions* stop_in_callback=nullptr;unsigned third_after=0;
        EffectSubmissions third(metadata,Refs(),Fields,Identify,[&](const auto&){stop_in_callback->Stop();},
            [&](const auto&,bool){++third_after;});stop_in_callback=&third;third.ActivateOnCurrentThread();
        Sync(&manager,&request,&info);Check(third_after==0 && roots.empty(),"stopped callbacks or roots survived");
        std::cout<<"Submission hooks: forwarding, provenance, nested calls, failures, retention and subscriptions passed.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
