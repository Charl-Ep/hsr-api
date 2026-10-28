#include <hksr/presentation/movement/AnimationMovementData.hpp>
#include <hksr/presentation/movement/AnimationMovementAccess.hpp>
#include <hksr/presentation/movement/OwnedMovementRequest.hpp>
#include <hksr/presentation/movement/MovementGeometry.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
bool Near(float a,float b){return std::abs(a-b)<0.00001f;}
template<class F>void Reject(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("invalid plan accepted");}
template<class T>T Read(void* object,std::size_t offset){T value;std::memcpy(&value,static_cast<char*>(object)+offset,sizeof(value));return value;}
template<class T>void Write(void* object,std::size_t offset,T value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof(value));}
template<class T>Method M(T code){return {reinterpret_cast<void*>(code),nullptr};}
struct Blob{alignas(void*) std::array<unsigned char,192> data{};};
// Relocated reference fields exercise declaration-based layout use. DDF stays
// the reviewed 32-byte inline value; its declaration adapter checks that ABI.
const AnimationMoveDataLayout layout{40,56,60,64,68,72,76,24,40,44,64,96,100,104};
struct Fixture {
    std::deque<Blob> blobs;
    std::map<int,void*> roots;
    std::map<void*,std::vector<void*>> lists;
    int next=0,allocations=0,retains=0,fail_retain_at=0,free_calls=0,fail_free=0;
    int ctor_calls=0,adds=0,submits=0,clears=0;
    bool retire_on_submit=false,current=true,finish_on_submit=false;
    bool evaluate_on_submit=false,fail_clear=false;
    bool alias=false;void* first=nullptr;
    ObjectResult Allocate() {
        ++allocations;
        if(alias && first)return {{true,0},first};
        blobs.emplace_back();auto* p=blobs.back().data.data();if(!first)first=p;return {{true,0},p};
    }
}* fixture;
int __fastcall Keep(void* value,int,int,void*) {
    if(++fixture->retains==fixture->fail_retain_at)return 0;
    const auto id=++fixture->next;fixture->roots[id]=value;return id;
}
void* __fastcall Target(int id,void*){return fixture->roots.at(id);}
void __fastcall Free(int id,void*) {
    ++fixture->free_calls;
    if(id==fixture->fail_free)throw std::runtime_error("release failed");
    fixture->roots.erase(id);
}
bool Rooted(void* p){for(const auto& v:fixture->roots)if(v.second==p)return true;return false;}
void __fastcall Constructor(void* value,void*){Check(Rooted(value),"constructor before root");++fixture->ctor_calls;}
void __fastcall ListConstructor(void* value,int capacity,void*) {
    Check(Rooted(value) && capacity>0,"list constructor ABI/root");
    fixture->lists[value].reserve(capacity);++fixture->ctor_calls;
}
void __fastcall Add(void* list,void* part,void*) {
    Check(Rooted(list)&&Rooted(part),"list add before root");
    auto* curve=Read<void*>(part,layout.curve);if(curve)Check(Rooted(curve),"curve not retained");
    fixture->lists.at(list).push_back(part);++fixture->adds;
}
AnimationMoveDataServices Services(Fixture& f) {
    fixture=&f;AnimationMoveDataServices s;s.references={M(Keep),M(Target),M(Free),{}, {}};
    s.layout=layout;s.data=s.segment=s.list=[&f]{return f.Allocate();};
    s.data_ctor=s.segment_ctor=M(Constructor);s.list_ctor=M(ListConstructor);s.list_add=M(Add);return s;
}
AnimationMovePlan Input() {
    AnimationMovePlan p;p.origin={1,2,3};p.animation_key=-123;p.force_start=true;
    p.segments={{{4,2,3},0.1f,0.4f,3,-1,nullptr},{{1,2,3},1.2f,1.8f,1,0.9f,nullptr}};return p;
}
void PlanTests() {
    auto input=Input();auto p=CompileAnimationMove(input);
    Check(p.duration==0 && Near(p.end_time,1.8f) && p.animation_key==-123 && p.force_start,"animation clock/key");
    Check(Near(p.segments[1].start,1.2f)&&p.segments[1].origin.x==4&&p.segments[1].target.x==1,"private return anchor/interval");
    Check(p.segments[1].exit_time==0.9f,"exit time retained");
    input.clock=MovementClock::DistanceSpeed;p=CompileAnimationMove(input);
    Check(Near(p.duration,4)&&p.end_time==1,"distance/speed total");
    Check(p.segments[0].start==0&&Near(p.segments[0].end,0.25f)&&Near(p.segments[1].start,0.25f)&&p.segments[1].end==1,"speed-weighted intervals");
    // Literal recipe validation is independent of the current battle context.
    input=Input();input.animation_key=0;Check(CompileAnimationMove(input).animation_key==0,"zero key silently rewritten");
    input.segments.clear();Reject([&]{CompileAnimationMove(input);});
    input=Input();input.segments[0].end=input.segments[0].start;Reject([&]{CompileAnimationMove(input);});
    input=Input();input.origin.z=std::numeric_limits<float>::quiet_NaN();Reject([&]{CompileAnimationMove(input);});
    input=Input();input.clock=MovementClock::DistanceSpeed;input.segments[0].speed=0;Reject([&]{CompileAnimationMove(input);});
    input=Input();input.clock=MovementClock::DistanceSpeed;input.segments[0].target=input.origin;Reject([&]{CompileAnimationMove(input);});
    input=Input();input.clock=MovementClock::DistanceSpeed;input.segments[0].speed=std::numeric_limits<float>::denorm_min();Reject([&]{CompileAnimationMove(input);});
}
void GraphTests() {
    Fixture f;auto input=Input();int curve=1;input.segments[0].curve=&curve;
    AnimationMovementData a(Services(f)),b(Services(f));
    Check(a.Prepare(input)&&b.Prepare(input),"prepare data graph");
    auto* left=a.Object();auto* right=b.Object();
    Check(left&&right&&left!=right,"fresh data per request");
    auto* ll=Read<void*>(left,layout.segments);auto* rl=Read<void*>(right,layout.segments);
    Check(ll!=rl&&f.lists.at(ll).size()==2&&f.lists.at(rl).size()==2,"fresh lists");
    auto* first=f.lists.at(ll)[0];auto* second=f.lists.at(ll)[1];
    Check(first!=f.lists.at(rl)[0]&&second!=f.lists.at(rl)[1],"fresh segments");
    Check(Read<int>(left,layout.key)==-123&&Read<bool>(left,layout.force_start)&&Read<float>(left,layout.duration)==0,"data scalar layout");
    Check(Read<float>(left,layout.previous_time)==0&&Read<float>(left,layout.progress)==0,"initial clocks");
    Check(Read<Position3>(first,layout.origin).x==1&&Read<Position3>(second,layout.origin).x==4,"chained nominal origins");
    Check(Read<Position3>(second,layout.target_sample).x==1&&Read<float>(second,layout.target_sample+12)==1&&Read<float>(second,layout.target_sample+16)==0,"resolved return sample");
    const auto direction=Read<Position3>(second,layout.target_sample+20);
    Check(direction.x==0&&direction.y==0&&direction.z==0,"sample direction initialized");
    Check(Read<int>(first,layout.update_mode)==0&&Read<int>(second,layout.update_mode)==0,"no formation tracking");
    Check(Read<void*>(first,layout.curve)==&curve&&Read<float>(second,layout.exit_time)==0.9f,"curve/exit identity");
    Write(left,layout.key,0);Write(first,layout.segment_start,0.7f);
    Check(Read<int>(right,layout.key)==-123&&Near(Read<float>(f.lists.at(rl)[0],layout.segment_start),0.1f),"mutable engine state shared");
    Check(!a.Prepare(input),"same graph reconstructed");
    Check(a.Close()&&!a.Object()&&b.Object()&&Rooted(&curve),"one graph cleanup erased other roots");
    Check(b.Close()&&f.roots.empty(),"all graph roots released");
    const auto freed=f.free_calls;Check(b.Close()&&freed==f.free_calls,"cleanup repeated frees");
}
void FailureTests() {
    {
        Fixture f;AnimationMovementData data(Services(f));auto input=Input();input.segments.clear();
        Check(!data.Prepare(input)&&f.allocations==0&&!data.Object()&&!data.Error().empty(),"invalid plan allocated native data");
    }
    {
        Fixture f;f.fail_retain_at=3;AnimationMovementData data(Services(f));
        Check(!data.Prepare(Input())&&!data.Object()&&f.adds==0&&f.roots.empty(),"partial prepare escaped/left roots");
    }
    {
        Fixture f;f.alias=true;AnimationMovementData data(Services(f));
        Check(!data.Prepare(Input())&&f.ctor_calls==1&&f.roots.empty(),"aliased allocator mutated existing allocation");
    }
    {
        Fixture f;AnimationMovementData data(Services(f));Check(data.Prepare(Input()),"cleanup fixture");f.fail_free=1;
        Check(!data.Close()&&!data.Object(),"unknown root release treated as closed");const auto calls=f.free_calls;
        Check(!data.Close()&&calls==f.free_calls,"unknown release retried");
    }
}
void LayoutTests() {
    Blob component;
    Write(&component,48,Position3{1,2,3});
    MetadataResolver geometry_methods({{"RPG.GameCore","CharacterDataComponent",{
        {"get_VisualRadius","System.Single",{},false,M(+[](void*,void*)->float{return 1.25f;})}}}});
    std::vector<DeclaredField> geometry_fields{{"RPG.GameCore","CharacterDataComponent","<LocalOffsetAsMoveTarget>k__BackingField","RPG.MVector3",48,1,true}};
    const auto geometry=ResolveMovementGeometryAccess(geometry_methods,geometry_fields);
    const auto geometry_value=geometry.Read(&component);
    Check(geometry_value.read.returned && geometry_value.value.radius==1.25f && geometry_value.value.local_offset.x==1 &&
        geometry_value.value.local_offset.y==2 && geometry_value.value.local_offset.z==3,"movement geometry field/read contract");
    Check(!geometry.Read(nullptr).read.returned,"missing geometry treated as a successful zero input");
    geometry_fields.front().flags=0x10;
    Reject([&]{ResolveMovementGeometryAccess(geometry_methods,geometry_fields);});
    std::vector<DeclaredField> data,segment,sample;
    auto add=[](auto& rows,const char* owner,const char* name,const char* type,std::size_t offset) {
        rows.push_back({"",owner,name,type,offset,6,true});
    };
    add(data,"FGNNHFLGJBK","JONGMMPJEIA","System.Collections.Generic.List<BCNMLILBENC>",40);
    const char* names[]={"PIMDCJGDHMG","CNHMBFIHLPC","CDJLMJDEPJA","NOIIIOKMIIN","GJGKKECHEHH","KPHIFEFPPDD"};
    for(int i=0;i<6;++i)add(data,"FGNNHFLGJBK",names[i],i==3?"System.Int32":i==5?"System.Boolean":"System.Single",56+4*i);
    add(segment,"BCNMLILBENC","KLOKABEPKLL","RPG.GameCore.FloatCurve",24);
    add(segment,"BCNMLILBENC","CKIBNLPPIMG","System.Single",40);
    add(segment,"BCNMLILBENC","FJBPOFJPFNP","RPG.MVector3",44);
    add(segment,"BCNMLILBENC","AAKHOJDBHPH","DDFDNJCFIHM",64);
    add(segment,"BCNMLILBENC","FMHMAHCNDGB","System.Single",96);
    add(segment,"BCNMLILBENC","HGDKNBJHDAP","RPG.GameCore.MoveToPositionUpdateMode",100);
    add(segment,"BCNMLILBENC","NNNAGHMNFKB","System.Single",104);
    add(sample,"DDFDNJCFIHM","LJCENKGKELP","RPG.MVector3",16);
    add(sample,"DDFDNJCFIHM","HJLOCBLHNLD","System.Single",28);
    add(sample,"DDFDNJCFIHM","GNPIPAADIFK","System.Single",32);
    add(sample,"DDFDNJCFIHM","AIPNOJPDPCM","RPG.MVector3",36);
    const auto value=ResolveAnimationMoveDataLayout460(data,segment,sample);Check(value.key==68&&value.segment_start==104,"layout resolver");
    sample.back().offset=40;Reject([&]{ResolveAnimationMoveDataLayout460(data,segment,sample);});sample.back().offset=36;
    data.front().flags|=0x10;Reject([&]{ResolveAnimationMoveDataLayout460(data,segment,sample);});data.front().flags=6;
    segment.back().type="System.Int32";Reject([&]{ResolveAnimationMoveDataLayout460(data,segment,sample);});
}
// This fixture deliberately models submission's dangerous pre-key side effect:
// production must keep existing motion out of this call, not undo it afterward.
const AnimationMovementAccessBinding access_layout{{},{},16,24,32,40,48,56,57,60,layout};
enum class Evaluation { Waiting,Advance,Complete,Interrupt,Replace,Fault };
Evaluation evaluation=Evaluation::Advance;
void* replacement=nullptr;
int evaluator_calls=0,method_marker=0;
__declspec(noinline) bool __fastcall Evaluate(void* component,float delta,Position3* out,void* metadata);
void __fastcall Submit(void* component,void* data,void*) {
    ++fixture->submits;
    Write(component,access_layout.other_motion,static_cast<void*>(nullptr));
    Write(component,access_layout.data,fixture->finish_on_submit?nullptr:data);
    if(fixture->retire_on_submit)fixture->current=false;
    if(fixture->evaluate_on_submit){Position3 result;Evaluate(component,0,&result,&method_marker);}
}
void __fastcall Clear(void* component,void*) {
    ++fixture->clears;
    if(fixture->fail_clear)throw std::runtime_error("failed clear");
    auto* data=Read<void*>(component,access_layout.data);
    auto* list=Read<void*>(data,layout.segments);fixture->lists.at(list).clear();
    Write(data,layout.key,0);Write(component,access_layout.data,static_cast<void*>(nullptr));
    Write(component,access_layout.active,false);Write(component,access_layout.index,0);
}
__declspec(noinline) bool __fastcall Evaluate(void* component,float delta,Position3* out,void* metadata) {
    ++evaluator_calls;Check(metadata==&method_marker,"evaluator metadata modified");
    *out={delta,12,34};auto* data=Read<void*>(component,access_layout.data);
    if(evaluation==Evaluation::Fault)RaiseException(0xE0670007,0,0,nullptr);
    if(!data || evaluation==Evaluation::Waiting)return false;
    if(evaluation==Evaluation::Replace){Write(component,access_layout.data,replacement);return true;}
    if(evaluation==Evaluation::Interrupt){Clear(component,nullptr);return false;}
    Write(component,access_layout.active,true);
    Write(data,layout.previous_time,Read<float>(data,layout.previous_time)+delta);
    Write(data,layout.progress,Read<float>(data,layout.progress)+delta*2);
    if(evaluation==Evaluation::Complete)Clear(component,nullptr);
    return true;
}
bool FaultedEvaluation(void* component) {
    Position3 out;
    __try{Evaluate(component,0,&out,&method_marker);return false;}
    __except(GetExceptionCode()==0xE0670007?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
AnimationMovementAccess Access(int& actor){auto b=access_layout;b.submit=M(Submit);b.clear=M(Clear);return {b,reinterpret_cast<GameEntity*>(&actor)};}
void* Component(Blob& blob,int& actor,int& transform,int& animator) {
    auto* p=blob.data.data();Write(p,16,static_cast<void*>(&actor));Write(p,24,static_cast<void*>(&transform));Write(p,32,static_cast<void*>(&animator));return p;
}
void AccessTests() {
    Fixture f;AnimationMovementData a(Services(f)),b(Services(f));Check(a.Prepare(Input())&&b.Prepare(Input()),"access data fixture");
    Blob blob;int actor=1,transform=2,animator=3,foreign=4;auto* component=Component(blob,actor,transform,animator);
    const auto access=Access(actor);auto current=[&]{return f.current;};
    Write(component,access_layout.owner,static_cast<void*>(&foreign));
    Check(!access.Submit(component,a,current).attempted&&!access.Interrupt(component,a,current).attempted&&f.submits==0&&f.clears==0,"foreign component owner touched");
    Write(component,access_layout.owner,static_cast<void*>(&actor));
    f.current=false;Check(!access.Submit(component,a,current).attempted&&f.submits==0,"ineligible owner written");f.current=true;
    Write(component,access_layout.other_motion,static_cast<void*>(&foreign));
    Check(!access.Submit(component,a,current).attempted&&f.submits==0&&Read<void*>(component,40)==&foreign,"other channel cleared by eligibility check");
    Write(component,access_layout.other_motion,static_cast<void*>(nullptr));Write(component,access_layout.data,b.Object());
    Check(!access.Submit(component,a,current).attempted&&f.submits==0,"same-key foreign FGNN replaced");
    Write(component,access_layout.data,static_cast<void*>(nullptr));Write(component,access_layout.attached,true);
    Check(!access.Submit(component,a,current).attempted&&f.submits==0,"effect motion overwritten");Write(component,access_layout.attached,false);
    const auto posted=access.Submit(component,a,current);Check(posted.attempted&&posted.call.returned&&posted.state==MovementWriteState::Owned,"submission/adoption");
    const auto clock=access.Clock(a);Check(clock.read.returned&&clock.key==-123&&clock.movement_time==0,"owned graph clock");
    Write(component,access_layout.data,b.Object());
    const auto replaced=access.Interrupt(component,a,current);Check(replaced.state==MovementWriteState::Replaced&&!replaced.attempted&&f.clears==0,"old request cleared successor");
    Write(component,access_layout.data,a.Object());Write(component,access_layout.other_motion,static_cast<void*>(&foreign));
    const auto stopped=access.Interrupt(component,a,current);Check(stopped.state==MovementWriteState::Detached&&stopped.attempted&&f.clears==1,"scoped interruption");
    Check(Read<void*>(component,access_layout.other_motion)==&foreign,"FGNN interruption cleared another channel");
    Check(access.Interrupt(component,a,current).state==MovementWriteState::Detached&&f.clears==1,"empty clear invoked");
    Write(component,access_layout.other_motion,static_cast<void*>(nullptr));
    Check(!access.Submit(component,a,current).attempted&&f.submits==1,"cleared graph resubmitted");
    Check(a.Close()&&b.Close()&&f.roots.empty(),"access fixtures drained");
    // A native entry can end a short request synchronously. Detachment does not
    // become a fabricated natural-completion receipt.
    AnimationMovementData c(Services(f));Check(c.Prepare(Input()),"immediate fixture");f.finish_on_submit=true;
    Check(access.Submit(component,c,current).state==MovementWriteState::Detached,"synchronous detachment");Check(c.Close(),"immediate roots");
    AnimationMovementData d(Services(f));Check(d.Prepare(Input()),"retirement fixture");f.finish_on_submit=false;f.retire_on_submit=true;
    const auto retired=access.Submit(component,d,current);Check(retired.state==MovementWriteState::Failed&&retired.attempted,"retirement mislabeled success");
    // Model destruction is the external owner in this case; no follow-up write
    // through the retired component is permitted by the adapter.
    Check(!access.Interrupt(component,d,current).attempted&&f.clears==1,"retired component touched");
    Write(component,access_layout.data,static_cast<void*>(nullptr));Check(d.Close(),"retired roots");
}
void RequestTests() {
    Fixture f;Blob blob;int actor=1,transform=2,animator=3;auto* component=Component(blob,actor,transform,animator);
    EntityLifecycle lifecycle;auto token=lifecycle.Track(reinterpret_cast<GameEntity*>(&actor));
    auto make=[&](std::shared_ptr<OwnedMovementHost> host={}) {
        MovementRequestServices s;s.data=Services(f);s.access=access_layout;s.access.submit=M(Submit);s.access.clear=M(Clear);s.update=M(Evaluate);
        s.lifecycle=&lifecycle;s.current=[&]{return f.current && !lifecycle.IsRetiring(token);};
        s.alive=[&]{return lifecycle.Current(token.entity).generation==token.generation && !lifecycle.IsDisposed(token);};
        s.host=std::move(host);
        return std::make_unique<OwnedMovementRequest>(s,token,component);
    };
    auto tick=[&](Evaluation next) {
        evaluation=next;Position3 out;const bool result=Evaluate(component,0.25f,&out,&method_marker);
        Check(out.x==0.25f&&out.y==12&&out.z==34,"hook changed output");return result;
    };
    {
        auto request=make();request->Start(Input());Check(request->Status().accepted,"request not adopted");
        Check(!tick(Evaluation::Waiting),"waiting return changed");request->Service();Check(!request->Status().started&&!request->Status().closed,"wait treated as completed");
        Check(tick(Evaluation::Advance),"advance return changed");request->Service();
        Check(request->Status().started&&request->Status().animation_time==0.25f&&request->Status().movement_time==0.5f,"official clock not observed");
        Check(tick(Evaluation::Complete),"completion return changed");request->Service();
        Check(request->Status().completed&&request->Status().closed&&!request->Status().cancelled&&request->Status().error.empty(),"natural completion missing");
    }
    {
        auto request=make();request->Start(Input());tick(Evaluation::Interrupt);request->Service();
        Check(request->Status().interrupted&&!request->Status().completed&&request->Status().closed&&!request->Status().error.empty(),"animation change fabricated completion");
    }
    {
        auto request=make();request->Start(Input());tick(Evaluation::Advance);Clear(component,nullptr);request->Service();
        Check(!request->Status().completed&&request->Status().closed&&!request->Status().error.empty(),"external clear fabricated completion");
    }
    {
        auto request=make();request->Start(Input());AnimationMovementData newer(Services(f));Check(newer.Prepare(Input()),"replacement fixture");
        replacement=newer.Object();tick(Evaluation::Replace);const auto clears=f.clears;request->Cancel();request->Service();
        Check(request->Status().replaced&&request->Status().closed&&!request->Status().completed&&f.clears==clears,"successor cleared");
        Write(component,access_layout.data,static_cast<void*>(nullptr));Check(newer.Close(),"successor cleanup");
    }
    {
        auto request=make();request->Start(Input());const auto clears=f.clears;f.current=false;request->Service();f.current=true;
        Check(request->Status().cancelled&&request->Status().closed&&f.clears==clears+1,"model change not scoped-cleared");
    }
    {
        auto request=make();request->Start(Input());const auto clears=f.clears;
        const auto notice=lifecycle.DisposalStarting(token);
        Check(notice.invoked==1&&notice.failed==0&&request->Status().closed&&f.clears==clears+1,"pre-disposal did not drain");
        lifecycle.Disposed(token);lifecycle.Forget(token);token=lifecycle.Track(reinterpret_cast<GameEntity*>(&actor));
    }
    {
        auto request=make();request->Start(Input());const auto clears=f.clears;
        lifecycle.Disposed(token);request->Service();
        Check(request->Status().closed&&request->Status().owner_lost&&f.clears==clears,"disposed component written");
        Write(component,access_layout.data,static_cast<void*>(nullptr));lifecycle.Forget(token);token=lifecycle.Track(reinterpret_cast<GameEntity*>(&actor));
    }
    {
        f.evaluate_on_submit=true;evaluation=Evaluation::Complete;auto request=make();request->Start(Input());f.evaluate_on_submit=false;
        Check(request->Status().accepted&&request->Status().completed&&request->Status().closed,"synchronous natural completion lost");
    }
    {
        f.finish_on_submit=true;auto request=make();request->Start(Input());f.finish_on_submit=false;
        Check(!request->Status().completed&&request->Status().closed&&!request->Status().error.empty(),"unobserved synchronous clear passed");
    }
    {
        auto request=make();request->Start(Input());evaluation=Evaluation::Fault;
        Check(FaultedEvaluation(component),"original exception swallowed");request->Service();
        Check(!request->Status().completed&&request->Status().closed&&!request->Status().error.empty(),"fault not drained");
    }
    {
        auto request=make();request->Start(Input());f.fail_clear=true;request->Cancel();request->Service();const auto clears=f.clears;
        Check(!request->Status().closed&&!request->Status().error.empty(),"failed clear released graph");request->Service();
        Check(f.clears==clears,"failed native clear retried");f.fail_clear=false;Clear(component,nullptr);request->Service();
        Check(request->Status().closed,"late detachment not drained");
    }
    {
        auto request=make();request->Start(Input());auto conflict=make();conflict->Start(Input());
        Check(conflict->Status().closed&&!conflict->Status().accepted,"second request stole component");
        Blob foreign;Component(foreign,actor,transform,animator);evaluation=Evaluation::Advance;Position3 out;
        const auto calls=evaluator_calls;for(int i=0;i<100;++i)Check(!Evaluate(foreign.data.data(),0.5f,&out,&method_marker),"foreign result changed");
        Check(evaluator_calls==calls+100&&!request->Status().started,"foreign evaluator not forwarded");
        std::thread other([&]{Evaluate(component,0.5f,&out,&method_marker);});other.join();request->Service();
        Check(!request->Status().started,"wrong thread published observation");request->Cancel();request->Service();Check(request->Status().closed,"conflict fixture cleanup");
    }
    {
        bool root=true,callback=true,pending=true;
        auto host=[&] {
            MovementHostServices s;s.references=Services(f).references;
            s.current=[&]{return f.current && !lifecycle.IsRetiring(token);};
            s.alive=[&]{return !lifecycle.IsDisposed(token);};
            s.lookup=[&]{return ObjectResult{{true,0},component};};
            s.create=[]()->ObjectResult{throw std::runtime_error("unexpected create");};
            s.initialize=[](void*)->CallResult{throw std::runtime_error("unexpected init");};
            s.remove=[](void*)->CallResult{throw std::runtime_error("existing component removed");};
            s.observe=[&](void* c){return AnimationMovementAccess(access_layout,token.entity).Observe(c);};
            s.read_root_motion=[&]{return Observed<bool>{root,{true,0}};};
            s.write_root_motion=[&](bool v){root=v;return CallResult{true,0};};
            s.read_pending_motion=[&]{return Observed<bool>{pending,{true,0}};};
            s.read_callback=[&](void*){return Observed<bool>{callback,{true,0}};};
            s.write_callback=[&](void*,bool v){callback=v;return CallResult{true,0};};
            auto lease=std::make_shared<OwnedMovementHost>(s,token);Check(lease->Prepare(),"host fixture preparation");return lease;
        };
        auto request=make(host());const auto submits=f.submits;request->Start(Input());
        Check(request->Status().waiting_for_host && f.submits==submits && !root,"request bypassed pending motion");
        pending=false;request->Service();Check(request->Status().accepted && f.submits==submits+1,"drained host not submitted");
        tick(Evaluation::Complete);request->Service();Check(request->Status().closed && root && callback,"completion did not restore motion");
        pending=true;auto cancelled=make(host());cancelled->Start(Input());const auto before=f.submits;
        cancelled->Cancel();cancelled->Service();Check(cancelled->Status().closed && f.submits==before && root && callback,"pending cancel submitted movement");
        auto handoff=make(host());handoff->Start(Input());f.current=false;handoff->Service();f.current=true;
        Check(handoff->Status().closed && root && callback && f.submits==before,"pending model handoff did not restore");
        auto retired=make(host());retired->Start(Input());const auto notice=lifecycle.DisposalStarting(token);
        Check(notice.invoked==1 && retired->Status().closed && root && callback,"pending retirement did not drain");
    }
    Check(f.roots.empty(),"request roots leaked");
}
}
int main(){try{PlanTests();GraphTests();FailureTests();LayoutTests();AccessTests();RequestTests();std::cout<<"movement data/access/native-observation/lifetime tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
