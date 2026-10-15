#include <hksr/world/WorldFrames.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../platform/NativeHooks.hpp"
#include <stdexcept>
#include <string>
#include <atomic>

namespace hksr::sdk_v1 {
namespace {
WorldFrameMethods bindings;
WorldFrameSink receiver;
detail::NativeHooks hooks;
thread_local bool delivering = false;
std::atomic<std::uint64_t> sequence{0};
struct DeliveryScope {
    DeliveryScope() { delivering=true; }
    ~DeliveryScope() { delivering=false; }
};
template<class T> Observed<T> Get(Method method, GameWorld* world) {
    Observed<T> out;
    out.read=native::Call(method,&out.value,world);
    return out;
}
void __fastcall Late(GameWorld* world, float dt, void* metadata) {
    reinterpret_cast<void(__fastcall*)(GameWorld*,float,void*)>(hooks.Original(0))(world,dt,metadata);
    // Reentrant official updates still execute, but cannot recursively service
    // the same framework request queue from its own callback.
    if (delivering || !receiver.consume) return;
    DeliveryScope delivery;
    try {
        WorldFrame frame;
        frame.sequence=++sequence;
        frame.world=world;frame.delta_time=dt;frame.thread_id=GetCurrentThreadId();
        frame.manager=Get<EntityManager*>(bindings.manager,world);
        frame.disposing=Get<bool>(bindings.disposing,world);
        frame.battle=Get<bool>(bindings.battle,world);
        frame.paused=Get<bool>(bindings.paused,world);
        frame.in_tick=Get<bool>(bindings.in_tick,world);
        if (world && bindings.has_phase_offset)
            frame.adventure_phase.read.returned=native::CopyMemoryChecked(&frame.adventure_phase.value,
                reinterpret_cast<const unsigned char*>(world)+bindings.phase_offset,sizeof(void*));
        // Even unready/disposing worlds are delivered. Classification and
        // which independent commands can proceed belong to the consumer.
        receiver.consume(frame,receiver.context);
    } catch(const std::exception& error) {
        if(receiver.failure)receiver.failure(error.what(),receiver.context);
    } catch(...) {
        if(receiver.failure)receiver.failure("world-frame consumer failed",receiver.context);
    }
}
}
WorldFrameMethods ResolveWorldFrameMethods(MethodResolver& source,const std::vector<DeclaredField>& fields) {
    WorldFrameMethods result;
    auto optional=[&](const char* name,const char* returns,std::initializer_list<const char*> parameters=std::initializer_list<const char*>{}) {
        try{return source.Resolve("RPG.GameCore","GameWorld",name,returns,parameters);}
        catch(const std::exception& error){result.binding_errors.push_back(std::string(name)+": "+error.what());return Method{};}
    };
    result.late_update=optional("LateUpdate","System.Void",{"System.Single"});
    result.manager=optional("get_EntityManagerRef","RPG.GameCore.EntityManager");
    result.disposing=optional("get_IsDisposing","System.Boolean");
    result.battle=optional("get_IsBattleGameWorld","System.Boolean");
    result.paused=optional("get_IsPauseTick","System.Boolean");
    result.in_tick=optional("get_IsInTick","System.Boolean");
    unsigned matches=0;
    for(const auto& field:fields)
        if(field.owner_namespace=="RPG.GameCore" && field.owner_name=="GameWorld" &&
            field.name=="BindAdventurePhase" && field.type=="RPG.Client.AdventurePhase"){
            ++matches;result.phase_offset=field.offset;
        }
    result.has_phase_offset=matches==1;
    if(!result.has_phase_offset)result.binding_errors.push_back("BindAdventurePhase: missing or ambiguous declared field");
    return result;
}
void InstallWorldFrameHook(const WorldFrameMethods& methods,WorldFrameSink sink) {
    if(hooks.Pending())throw std::logic_error("world-frame hook already installed");
    if(!methods.late_update.code || !sink.consume)throw std::invalid_argument("world hook needs a target and consumer");
    bindings=methods;receiver=sink;
    hooks.Install({{"GameWorld.LateUpdate", methods.late_update.code, reinterpret_cast<void*>(&Late)}});
}
void RemoveWorldFrameHookAtQuiescentPoint() {
    hooks.Remove(); bindings={}; receiver={};
}
}
