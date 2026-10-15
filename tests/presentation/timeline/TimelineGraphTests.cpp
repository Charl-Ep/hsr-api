#include "../../../src/presentation/timeline/TimelineGraphAccess.hpp"
#include "../../../src/presentation/timeline/TimelineEnvironment.hpp"
#include <Windows.h>
#include <map>
#include <iostream>
#include <thread>
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
namespace {
void Check(bool ok){if(!ok)throw std::runtime_error("Timeline graph ABI test failed");}
int graph_info=1,valid_info=2,resolver_info=3;
int director=0,other=0;void* result=&director;
PlayableToken expected{0x123456789abcdef0,47,0xfedcba98},graph_value{0x234567890abcdef1,31,0x12345678};
bool valid=true,fail_graph=false,fail_resolver=false,fail_free=false;
int graph_calls=0,resolver_calls=0;std::map<int,void*> roots;int seq=0;
std::map<int,int> root_kinds;
struct Behaviour {int starts=0,updates=0,exits=0,restores=0;};
struct Frame {unsigned char data[64]{};};
std::vector<Behaviour*> deferred;
const void* seen_playable=nullptr;const void* seen_frame=nullptr;void* seen_data=nullptr;void* seen_info=nullptr;
bool throw_original=false;int callback_info[9]={};
void Original(std::size_t index,Behaviour* b,const void* p,const void* f,void* data,void* info) {
    Check(info==&callback_info[index]);seen_playable=p;seen_frame=f;seen_data=data;seen_info=info;
    if(index%3==0)++b->starts;
    else if(index%3==1)++b->updates;
    else {++b->exits;deferred.push_back(b);}
    if(throw_original)RaiseException(0xe0760804,0,0,nullptr);
}
template<std::size_t I>__declspec(noinline) void __fastcall BehaviourCall(Behaviour* b,const void* p,const void* f,void* info){Original(I,b,p,f,nullptr,info);}
template<std::size_t I>__declspec(noinline) void __fastcall ProcessCall(Behaviour* b,const void* p,const void* f,void* data,void* info){Original(I,b,p,f,data,info);}
template<std::size_t I>__declspec(noinline) void __fastcall GraphCall(Behaviour* b,const void* p,void* info){Original(I,b,p,nullptr,nullptr,info);}
void* callbacks[9]={reinterpret_cast<void*>(&BehaviourCall<0>),reinterpret_cast<void*>(&ProcessCall<1>),reinterpret_cast<void*>(&BehaviourCall<2>),
    reinterpret_cast<void*>(&BehaviourCall<3>),reinterpret_cast<void*>(&ProcessCall<4>),reinterpret_cast<void*>(&BehaviourCall<5>),
    reinterpret_cast<void*>(&GraphCall<6>),reinterpret_cast<void*>(&ProcessCall<7>),reinterpret_cast<void*>(&GraphCall<8>)};
DWORD Invoke(std::size_t index,Behaviour* b,const void* p,const void* f,void* data=nullptr) {
    __try {
        if(index%3==1)reinterpret_cast<void(__fastcall*)(Behaviour*,const void*,const void*,void*,void*)>(callbacks[index])(b,p,f,data,&callback_info[index]);
        else if(index>=6)reinterpret_cast<void(__fastcall*)(Behaviour*,const void*,void*)>(callbacks[index])(b,p,&callback_info[index]);
        else reinterpret_cast<void(__fastcall*)(Behaviour*,const void*,const void*,void*)>(callbacks[index])(b,p,f,&callback_info[index]);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void Drain(){for(auto* b:deferred)++b->restores;deferred.clear();}
void __fastcall GetGraph(PlayableToken* p,PlayableToken* out,void* info) {
    ++graph_calls;Check(info==&graph_info && *p==expected && p->padding==expected.padding);
    if(fail_graph)RaiseException(0xe0760801,0,0,nullptr);
    *out=graph_value;p->version=100; // caller's original temporary must survive
}
bool __fastcall IsValid(PlayableToken* p,void* info){Check(info==&valid_info && *p==graph_value);return valid;}
void* __fastcall Resolver(PlayableToken* p,void* info) {
    ++resolver_calls;Check(info==&resolver_info && *p==graph_value);
    if(fail_resolver)RaiseException(0xe0760802,0,0,nullptr);
    return result;
}
int __fastcall Keep(void* p,int handle,int kind,void*){Check(!handle && (kind==2 || kind==0));roots[++seq]=p;root_kinds[seq]=kind;return seq;}
void* __fastcall Target(int h,void*){return roots.at(h);}
void __fastcall Free(int h,void*){if(fail_free)RaiseException(0xe0760803,0,0,nullptr);roots.erase(h);root_kinds.erase(h);}
template<class F>Method M(F f,void* info=nullptr){return {reinterpret_cast<void*>(f),info};}
struct Provider final:MethodResolver,ObjectMetadata {
    Method Resolve(const char* ns,const char* type,const char* name,const char* return_type,std::initializer_list<const char*> params,MethodKind kind) override {
        std::vector<std::string> p;for(const auto* v:params)p.emplace_back(v);
        const std::string n=name,t=type,r=return_type;
        if(std::string(ns).find("RPGTools.")==0) {
            const std::size_t group=t=="LightControlBehaviour"?0:t=="SceneLightControlBehaviour"?1:2;
            Check(group!=2 || t=="EnviroSysAdjustLevelBehaviour");
            const std::size_t phase=n=="ProcessFrame"?1:(n=="OnBehaviourPlay" || n=="OnGraphStart")?0:2;
            Check(kind==MethodKind::Instance && r=="System.Void" && p[0]=="UnityEngine.Playables.Playable");
            Check(p.size()==(phase==1?3:group==2?1:2));
            if(p.size()>1)Check(p[1]=="UnityEngine.Playables.FrameData");
            if(p.size()>2)Check(p[2]=="System.Object");
            return {callbacks[group*3+phase],&callback_info[group*3+phase]};
        }
        Check(std::string(ns)=="UnityEngine.Playables" && kind==MethodKind::Static);
        if(n=="GetGraph_Injected") {
            Check(t=="PlayableHandle" && r=="System.Void" && p==std::vector<std::string>{"UnityEngine.Playables.PlayableHandle&","UnityEngine.Playables.PlayableGraph&"});
            return M(GetGraph,&graph_info);
        }
        Check(t=="PlayableGraph" && p==std::vector<std::string>{"UnityEngine.Playables.PlayableGraph&"});
        if(n=="IsValid_Injected"){Check(r=="System.Boolean");return M(IsValid,&valid_info);}
        Check(n=="GetResolver_Injected" && r=="UnityEngine.IExposedPropertyTable");return M(Resolver,&resolver_info);
    }
    DeclaredType ReadObjectType(void* p) override {Check(!roots.empty());return {"UnityEngine.Playables",p==&director?"PlayableDirector":"OtherResolver",{}};}
    std::vector<DeclaredField> ReadObjectFields(void*) override {throw std::logic_error("unused");}
};
std::vector<DeclaredField> Fields(const char*,const char* type) {
    if(std::string(type)=="Playable")return {{"UnityEngine.Playables",type,"m_Handle","UnityEngine.Playables.PlayableHandle",16,3,true}};
    return {{"UnityEngine.Playables",type,"m_Handle","System.IntPtr",16,3,true},{"UnityEngine.Playables",type,"m_Version","System.UInt32",24,3,true}};
}
}
void TestTimelineGraph() {
    Provider p;ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    TimelineGraphAccess reader(p,p,refs,Fields);
    auto value=expected;
    const auto row=reader.Read(&value);
    Check(row.playable==expected && value==expected && value.padding==expected.padding && row.graph==graph_value);
    Check(row.resolver==&director && row.resolver_is_director && roots.size()==1);
    result=&other;Check(!reader.Read(&value).resolver_is_director);
    result=nullptr;Check(!reader.Read(&value).resolver);
    bool failed=false;const auto before=graph_calls;
    try{reader.Read(nullptr);}catch(...){failed=true;}Check(failed && graph_calls==before);
    valid=false;failed=false;const auto before_resolver=resolver_calls;
    try{reader.Read(&value);}catch(...){failed=true;}Check(failed && resolver_calls==before_resolver);valid=true;
    fail_graph=true;failed=false;try{reader.Read(&value);}catch(...){failed=true;}Check(failed);fail_graph=false;
    fail_resolver=true;failed=false;try{reader.Read(&value);}catch(...){failed=true;}Check(failed);fail_resolver=false;
    Check(reader.Close() && roots.empty());failed=false;
    try{reader.Read(&value);}catch(...){failed=true;}Check(failed);
    auto bad=[](const char* ns,const char* t){auto f=Fields(ns,t);f[0].offset=0;return f;};
    failed=false;try{TimelineGraphAccess rejected(p,p,refs,bad);}catch(...){failed=true;}Check(failed);
    std::cout<<"Timeline graph injected ABI and resolver identity tests passed\n";
    result=&director;Frame frame;int user_data=0;TimelineOwner owner{1,2,3};
    bool allowed=false,official=false,unknown=false,source_fails=false,policy_fails=false;
    int source_calls=0,policy_calls=0;std::vector<std::string> errors;
    TimelineEnvironment environment(p,p,refs,Fields,[&](void* resolver)->std::optional<TimelinePlaybackSource> {
        ++source_calls;Check(resolver==&director);
        if(source_fails)throw std::runtime_error("source fixture failed");
        if(unknown)return {};
        return TimelinePlaybackSource{{{101,102},7,8},official?std::nullopt:std::optional<TimelineOwner>(owner)};
    },[&](TimelineOwner source,TimelineEnvironmentKind)->bool {
        ++policy_calls;Check(source==owner);
        if(policy_fails)throw std::runtime_error("policy fixture failed");
        return allowed;
    },[&](const auto& message){errors.push_back(message);});
    environment.ActivateOnCurrentThread();
    Behaviour denied[3];
    for(std::size_t g=0;g<3;++g) {
        const int before_sources=source_calls;
        Check(Invoke(g*3,&denied[g],&value,&frame)==0);
        Check(Invoke(g*3+1,&denied[g],&value,&frame,&user_data)==0);
        allowed=true; // must NOT turn on a cycle whose setup was suppressed
        Check(Invoke(g*3+1,&denied[g],&value,&frame,&user_data)==0);
        Check(Invoke(g*3+2,&denied[g],&value,&frame)==0);allowed=false;
        Check(denied[g].starts==0 && denied[g].updates==0 && denied[g].exits==0 && deferred.empty());
        Check(source_calls==before_sources+1);
    }
    Behaviour permitted[3];allowed=true;
    for(std::size_t g=0;g<3;++g) {
        Check(Invoke(g*3,&permitted[g],&value,&frame)==0);
        Check(seen_playable==&value && seen_frame==(g==2?nullptr:&frame) && seen_info==&callback_info[g*3]);
        Check(Invoke(g*3+1,&permitted[g],&value,&frame,&user_data)==0);
        Check(seen_playable==&value && seen_frame==&frame && seen_data==&user_data && seen_info==&callback_info[g*3+1]);
        Check(Invoke(g*3+2,&permitted[g],&value,&frame)==0);
        Check(permitted[g].starts==1 && permitted[g].updates==1 && permitted[g].exits==1);
    }
    Drain();for(const auto& b:permitted)Check(b.restores==1);allowed=false;
    Behaviour live;allowed=true;Check(Invoke(0,&live,&value,&frame)==0);
    Check(seen_playable==&value && seen_frame==&frame && seen_info==&callback_info[0]);
    allowed=false;Check(Invoke(1,&live,&value,&frame,&user_data)==0 && live.updates==1 && seen_data==&user_data);
    Check(Invoke(2,&live,&value,&frame)==0 && deferred.size()==1);
    // New denied cycle does not cancel an ALREADY queued official recovery.
    Check(Invoke(0,&live,&value,&frame)==0 && live.starts==1);Drain();Check(live.restores==1);
    Check(Invoke(2,&live,&value,&frame)==0 && deferred.empty());
    // Same address/new Playable generation cannot inherit old suppression.
    auto reused=value;++reused.version;Check(Invoke(1,&live,&reused,&frame)==0 && live.updates==2);
    // Late install/unknown start is not permission to suppress existing state.
    Behaviour late;Check(Invoke(2,&late,&value,&frame)==0 && late.exits==1);Drain();
    unknown=true;Check(Invoke(0,&live,&value,&frame)==0 && live.starts==2);unknown=false;
    Check(Invoke(2,&live,&value,&frame)==0 && live.exits==2);Drain();
    const auto policy_before=policy_calls;official=true;
    {TimelineCallScope outer(owner);Check(Invoke(0,&live,&value,&frame)==0 && live.starts==3);}
    official=false;Check(policy_calls==policy_before);
    source_fails=true;Check(Invoke(0,&live,&value,&frame)==0 && live.starts==4);source_fails=false;
    policy_fails=true;Check(Invoke(0,&live,&value,&frame)==0 && live.starts==5);policy_fails=false;
    Check(Invoke(2,&live,&value,&frame)==0 && live.exits==3);Drain();
    allowed=true;throw_original=true;Check(Invoke(0,&live,&value,&frame)==0xe0760804);throw_original=false;
    allowed=false;Check(Invoke(2,&live,&value,&frame)==0 && live.exits==4);Drain();
    // Wrong thread forwards untouched, without managed graph/source reads.
    const auto reads=source_calls;std::thread other_thread([&]{Check(Invoke(1,&live,&value,&frame)==0);});other_thread.join();Check(source_calls==reads);
    // Weak identity collection retires observations without rooting Behaviours.
    for(auto& h:roots)if(h.second==&denied[0]){Check(root_kinds.at(h.first)==0);h.second=nullptr;}
    Check(environment.Collect()==1);
    Check(Invoke(1,&denied[0],&value,&frame)==0 && denied[0].updates==1);
    Check(environment.Close() && roots.empty());
    const auto last=live.updates;Check(Invoke(1,&live,&value,&frame)==0 && live.updates==last+1);
    Check(!errors.empty());
    std::cout<<"Timeline environment candidate passed: nine real callbacks, cycle-paired policy, deferred allowed recovery, public/unknown/reuse/SEH/thread boundaries. Not game adoption.\n";
}
