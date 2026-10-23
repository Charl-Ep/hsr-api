#include <hksr/presentation/movement/AnimationMovementData.hpp>
#include <hksr/presentation/movement/AnimationMovementAccess.hpp>
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
void __fastcall Submit(void* component,void* data,void*) {
    ++fixture->submits;
    Write(component,access_layout.other_motion,static_cast<void*>(nullptr));
    Write(component,access_layout.data,fixture->finish_on_submit?nullptr:data);
    if(fixture->retire_on_submit)fixture->current=false;
}
void __fastcall Clear(void* component,void*) {
    ++fixture->clears;auto* data=Read<void*>(component,access_layout.data);
    auto* list=Read<void*>(data,layout.segments);fixture->lists.at(list).clear();
    Write(data,layout.key,0);Write(component,access_layout.data,static_cast<void*>(nullptr));
    Write(component,access_layout.active,false);Write(component,access_layout.index,0);
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
}
int main(){try{PlanTests();GraphTests();FailureTests();LayoutTests();AccessTests();std::cout<<"movement plan/data/access tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
