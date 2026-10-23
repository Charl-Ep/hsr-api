#include <hksr/presentation/movement/OwnedMovementHost.hpp>
#include <hksr/managed/GenericFactory.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

using namespace hksr::api;
namespace {
void Check(bool value){if(!value)throw std::runtime_error("movement host check failed");}
int handle=0,declaring=1,argument=2,type=3,text=4,definition=5,closed_method=6,entity=7,component=8,replacement=9;
std::map<int,void*> roots;
std::vector<void*> generic_args{&argument},input;
int find_count=0,close_count=0,invoke_count=0;
bool generic=true,open=false,invoke_fault=false;
int __fastcall Keep(void* p,int,int kind,void*){Check(kind==2);roots[++handle]=p;return handle;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
template<class F> Method M(F f){return {reinterpret_cast<void*>(f),nullptr};}
ReferenceMethods Refs(){return {M(Keep),M(Target),M(Free),{}, {}};}
void* __fastcall Find(void* t,void* n,void*){Check(t==&declaring && n==&text);++find_count;return &definition;}
bool __fastcall Definition(void* p,void*){Check(p==&definition);return generic;}
void* __fastcall Arguments(void* p,void*){Check(p==&definition);return &generic_args;}
void* __fastcall Make(void* p,void* a,void*){Check(p==&definition && a==&input && input==generic_args);++close_count;return &closed_method;}
bool __fastcall Open(void* p,void*){Check(p==&closed_method);return open;}
void* __fastcall Invoke(void* method,void* receiver,void* args,void*) {
    Check(method==&closed_method && receiver==&entity && !args);++invoke_count;
    if(invoke_fault)RaiseException(0xe0470011,0,0,nullptr);
    return &component;
}
void* __fastcall CreateArray(void* element,int count,void*){Check(element==&type && count==1);input.assign(1,nullptr);return &input;}
int __fastcall Length(void* p,void*){return static_cast<int>(static_cast<std::vector<void*>*>(p)->size());}
void __fastcall Set(void* p,void* item,int i,void*){static_cast<std::vector<void*>*>(p)->at(i)=item;}
struct Objects:ManagedObjects {
    ObjectResult NewStringUtf8(const char* value)override{Check(std::string(value)=="CreateComponent");return {{true,0},&text};}
    ObjectResult ManagedType(const char* ns,const char* name)override{Check(std::string(ns)=="System" && std::string(name)=="Type");return {{true,0},&type};}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("raw allocation is not component registration");}
};
void Reflection() {
    Objects objects;GenericFactoryMethods methods{M(Find),M(Definition),M(Arguments),M(Make),M(Open),M(Invoke)};
    ArrayMethods arrays;arrays.create=M(CreateArray);arrays.length=M(Length);arrays.set=M(Set);
    GenericFactory factory(methods,Refs(),objects,ManagedArrays(arrays));
    Check(factory.Prepare(&declaring,"CreateComponent",&argument));
    Check(factory.Prepare(&declaring,"CreateComponent",&argument));
    Check(!factory.Prepare(&declaring,"Other",&argument));
    Check(factory.Invoke(&entity).object==&component && factory.Invoke(&entity).call.returned);
    Check(find_count==1 && close_count==1 && invoke_count==2);
    invoke_fault=true;Check(!factory.Invoke(&entity).call.returned);invoke_fault=false;
    Check(factory.Close() && roots.empty() && !factory.Invoke(&entity).call.returned);
    generic=false;GenericFactory wrong(methods,Refs(),objects,ManagedArrays(arrays));
    Check(!wrong.Prepare(&declaring,"CreateComponent",&argument));Check(wrong.Close() && roots.empty());generic=true;
    open=true;GenericFactory unclosed(methods,Refs(),objects,ManagedArrays(arrays));
    Check(!unclosed.Prepare(&declaring,"CreateComponent",&argument));Check(unclosed.Close() && roots.empty());open=false;
}
struct Host {
    void* registered=nullptr;
    bool live=true,current=true,root=true,callback=true,pending=false,active=false;
    bool init_ok=true,create_ok=true,remove_ok=true,pause_ok=true;
    int created=0,initialized=0,removed=0,root_writes=0,callback_writes=0;
    MovementHostServices Services() {
        MovementHostServices s;s.references=Refs();
        s.current=[this]{return current;};s.alive=[this]{return live;};
        s.lookup=[this]{return ObjectResult{{true,0},registered};};
        s.create=[this]{++created;registered=&component;return ObjectResult{{create_ok,0},create_ok?&component:nullptr};};
        s.initialize=[this](void* c){Check(c==&component);++initialized;return CallResult{init_ok,0};};
        s.remove=[this](void* c){Check(c==&component && registered==c);++removed;if(remove_ok)registered=nullptr;return CallResult{remove_ok,0};};
        s.observe=[this](void*){AnimationMovementObservation o;o.read.returned=true;o.owner=&entity;o.transform=&type;o.animator_source=&argument;if(active)o.data=&text;return o;};
        s.read_root_motion=[this]{return Observed<bool>{root,{true,0}};};
        s.write_root_motion=[this](bool v){++root_writes;root=v;return CallResult{v || pause_ok,0};};
        s.read_pending_motion=[this]{return Observed<bool>{pending,{true,0}};};
        s.read_callback=[this](void*){return Observed<bool>{callback,{true,0}};};
        s.write_callback=[this](void* c,bool v){Check(c==&component);++callback_writes;callback=v;return CallResult{true,0};};
        return s;
    }
    EntityToken Token(){return {reinterpret_cast<GameEntity*>(&entity),1};}
};
void Lifecycle() {
    {Host h;OwnedMovementHost x(h.Services(),h.Token());Check(x.Prepare() && x.Ready());
     Check(!h.root && !h.callback && h.created==1 && h.initialized==1);Check(x.Prepare() && h.created==1);
     h.active=true;Check(!x.Close() && !h.root && h.removed==0);h.active=false;
     Check(x.Close() && h.root && h.removed==1 && roots.empty());Check(!x.Prepare() && !x.Ready() && !x.Component());}
    {Host h;h.registered=&component;h.pending=true;OwnedMovementHost x(h.Services(),h.Token());
     Check(x.Prepare() && !x.Ready() && x.Status().waiting && h.created==0 && h.initialized==0);
     h.pending=false;Check(x.Ready());Check(x.Close() && h.root && h.callback && h.removed==0 && roots.empty());}
    {Host h;h.init_ok=false;OwnedMovementHost x(h.Services(),h.Token());
     Check(!x.Prepare() && x.Status().closed && h.removed==1 && h.root_writes==0 && roots.empty());}
    {Host h;h.create_ok=false;OwnedMovementHost x(h.Services(),h.Token());
     Check(!x.Prepare() && x.Status().closed && h.removed==1 && h.initialized==0 && roots.empty());}
    {Host h;h.pause_ok=false;OwnedMovementHost x(h.Services(),h.Token());
     Check(!x.Prepare() && x.Status().closed && h.root && h.removed==1 && roots.empty());}
    {Host h;OwnedMovementHost x(h.Services(),h.Token());Check(x.Prepare());h.registered=&replacement;
     Check(x.Close() && x.Status().component_replaced && h.removed==0 && h.root && roots.empty());}
    {Host h;h.registered=&component;h.active=true;OwnedMovementHost x(h.Services(),h.Token());
     Check(!x.Prepare() && h.removed==0 && h.root_writes==0 && roots.empty());}
    {Host h;OwnedMovementHost x(h.Services(),h.Token());Check(x.Prepare());h.live=false;
     Check(x.Close() && x.Status().owner_lost && h.removed==0 && h.root_writes==1 && roots.empty());}
    {Host h;OwnedMovementHost x(h.Services(),h.Token());Check(x.Prepare());h.remove_ok=false;
     Check(!x.Close() && !x.Close() && h.removed==1 && !roots.empty());
     h.live=false;Check(x.Close() && roots.empty());}
    {Host h;h.current=false;OwnedMovementHost x(h.Services(),h.Token());
     Check(!x.Prepare() && h.created==0 && h.root_writes==0 && roots.empty());}
}
}
int main(){try{Reflection();Lifecycle();std::cout<<"Movement host construction, rollback, drain and isolation tests passed.\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
