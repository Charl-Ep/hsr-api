#include <hksr/world/WorldFrames.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <thread>

void TestModelRelease();

namespace {
using namespace hksr::api;
struct TestWorld { void* phase; };
int phase,manager,metadata;
TestWorld model{&phase};
auto* world=reinterpret_cast<GameWorld*>(&model);
unsigned originals=0,consumed=0,failures=0;
std::uint64_t previous_sequence=0;
bool original_throws=false,consumer_throws=false,disposing=false,reenter=false;
DWORD original_thread=0;
void Check(bool value){if(!value)throw std::runtime_error("world-frame test failed");}
__declspec(noinline) void __fastcall Original(GameWorld* value,float dt,void* info){
    Check(value==world && dt==0.125f && info==&metadata);++originals;original_thread=GetCurrentThreadId();
    if(original_throws)RaiseException(0xE0440001,0,0,nullptr);
}
EntityManager* __fastcall Manager(GameWorld* value,void*){Check(value==world);return reinterpret_cast<EntityManager*>(&manager);}
bool __fastcall Disposing(GameWorld* value,void*){Check(value==world);return disposing;}
bool __fastcall Battle(GameWorld*,void*){return false;}
bool __fastcall InTick(GameWorld*,void*){return true;}
bool InvokeOriginal(){
    __try{Original(world,0.125f,&metadata);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Consume(const WorldFrame& frame,void* context){
    Check(frame.sequence>previous_sequence); previous_sequence=frame.sequence;
    Check(context==&phase && frame.world==world && originals>consumed);++consumed;
    Check(GetCurrentThreadId()==original_thread && frame.thread_id==original_thread && frame.delta_time==0.125f);
    Check(frame.manager.read.returned && frame.manager.value==reinterpret_cast<EntityManager*>(&manager));
    Check(frame.adventure_phase.read.returned && frame.adventure_phase.value==&phase);
    Check(frame.disposing.read.returned && frame.disposing.value==disposing);
    Check(!frame.paused.read.returned && frame.in_tick.read.returned && frame.in_tick.value);
    if(reenter){reenter=false;Original(world,0.125f,&metadata);}
    if(consumer_throws)throw std::runtime_error("intentional consumer failure");
}
void Failure(const char*,void*) noexcept {++failures;}
struct Resolver:MethodResolver {
    unsigned attempts=0;
    Method Resolve(const char*,const char*,const char* name,const char*,std::initializer_list<const char*>,MethodKind) override{
        ++attempts;std::string key=name;void* function=nullptr;
        if(key=="LateUpdate")function=reinterpret_cast<void*>(&Original);
        else if(key=="get_EntityManagerRef")function=reinterpret_cast<void*>(&Manager);
        else if(key=="get_IsDisposing")function=reinterpret_cast<void*>(&Disposing);
        else if(key=="get_IsBattleGameWorld")function=reinterpret_cast<void*>(&Battle);
        else if(key=="get_IsInTick")function=reinterpret_cast<void*>(&InTick);
        else throw std::runtime_error("optional pause getter not supplied");
        return {function,&metadata};
    }
};
}
void TestWorldFrames(){
    Resolver resolver;
    const auto methods=ResolveWorldFrameMethods(resolver,{{"RPG.GameCore","GameWorld","BindAdventurePhase","RPG.Client.AdventurePhase",offsetof(TestWorld,phase)}});
    Check(resolver.attempts==6 && methods.binding_errors.size()==1);
    InstallWorldFrameHook(methods,{Consume,Failure,&phase});
    Check(InvokeOriginal() && consumed==1 && originals==1);
    TestModelRelease(); // independent group install/remove while world hook stays active
    disposing=true;Check(InvokeOriginal() && consumed==2); // non-ready frame is delivered
    consumer_throws=true;Check(InvokeOriginal() && failures==1 && consumed==3);
    consumer_throws=false;original_throws=true;
    Check(!InvokeOriginal() && consumed==3 && failures==1); // original error was not rewritten
    original_throws=false;reenter=true;
    Check(InvokeOriginal() && consumed==4 && originals==6);
    // A different calling thread is still reported, not silently filtered out.
    std::thread other([]{Original(world,0.125f,&metadata);});other.join();Check(consumed==5);
    RemoveWorldFrameHookAtQuiescentPoint();
    Check(InvokeOriginal() && consumed==5);
    std::cout<<"World frame hook tests passed: original ordering, field results, errors, reentry and thread delivery.\n";
}
