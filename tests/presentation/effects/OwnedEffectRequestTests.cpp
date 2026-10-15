#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
int info,entity,path,mapped,manager,other_manager,effect;
struct Params {unsigned id=0;bool released=false;int state=0;void* callback=nullptr;void* target=nullptr;};
Params first,second;Params* acquiring=&first;
std::map<int,void*> roots;int root_id=0;
std::vector<std::unique_ptr<std::array<unsigned char,128>>> delegates;
EffectCompletionLayout layout{24,40,64};
int submits=0,removes=0,releases=0;
bool sync=false,submit_fault=false,configure_fault=false,remove_match=true,release_on_remove=false,id_fault=false;
void* current_manager=&manager;void* last_removed_manager=nullptr;
void Check(bool value){if(!value)throw std::runtime_error("owned effect request test failed");}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),&info};}
int __fastcall Keep(void* object,int,int,void*){roots[++root_id]=object;return root_id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
void Invoke(Params& value,void* instance,void* parameter_override=nullptr) {
    void* entry=nullptr;void* cookie=nullptr;
    std::memcpy(&entry,static_cast<char*>(value.callback)+layout.invoke_impl,sizeof(entry));
    std::memcpy(&cookie,static_cast<char*>(value.callback)+layout.method_code,sizeof(cookie));
    reinterpret_cast<void(__fastcall*)(void*,void*,void*,void*)>(entry)(cookie,instance,parameter_override?parameter_override:&value,nullptr);
}
void* __fastcall Map(void* owner,void* name,void*){Check(owner==&entity && name==&path);return &mapped;}
bool __fastcall Empty(void*,void*){return false;}
EffectReference __fastcall Acquire(void* name,void*){Check(name==&mapped);return {acquiring->id,0,acquiring};}
unsigned __fastcall Id(Params* value,void*){if(id_fault)RaiseException(0xE0680002,0,0,nullptr);return value->id;}
bool __fastcall Released(Params* value,void*){return value->released;}
int __fastcall State(Params* value,void*){return value->state;}
void* __fastcall Instance(Params* value,void*){return value->state==3?&effect:nullptr;}
void __fastcall Submit(void* service,Params* value,void*) {
    Check(service==current_manager && value->target==&entity && value->callback);++submits;value->state=1;
    if(sync){Invoke(*value,&effect);Check(removes==0);value->state=3;}
    if(submit_fault)RaiseException(0xE0680001,0,0,nullptr);
}
bool __fastcall Remove(void* service,EffectReference ref,bool,void*) {
    ++removes;last_removed_manager=service;auto& value=*static_cast<Params*>(ref.parameters);
    if(!remove_match || value.id!=ref.id)return false;
    value.state=6;
    if(release_on_remove){Invoke(value,nullptr);value.released=true;}
    return true;
}
void __fastcall Release(Params* value,void*){++releases;value->released=true;}
EffectRequestServices Services() {
    return {{M(Keep),M(Target),M(Free),{},{}},
        EffectAccess({M(Map),M(Acquire),M(Submit),M(Remove),M(Release),M(Id),M(Released),M(State),M(Instance),{},M(Empty)}),
        {[]{auto data=std::make_unique<std::array<unsigned char,128>>();data->fill(0);auto* p=data->data();delegates.push_back(std::move(data));return ObjectResult{{true,0},p};},layout},
        []{return ObjectResult{{true,0},current_manager};},
        [](void* params,void* target,void* callback){auto& value=*static_cast<Params*>(params);value.target=target;
            if(configure_fault)return EffectInputResult{{true,0},{false,0xE0680003}};
            value.callback=callback;return EffectInputResult{{true,0},{true,0}};}};
}
void Reset() {
    Check(roots.empty());delegates.clear();first={};second={19};acquiring=&first;
    submits=removes=releases=0;sync=submit_fault=configure_fault=release_on_remove=id_fault=false;
    remove_match=true;current_manager=&manager;last_removed_manager=nullptr;
}
}
void TestOwnedEffectRequest() {
    Reset();
    {
        std::vector<std::string> traces;
        auto services=Services();services.trace=[&](const std::string& line){traces.push_back(line);};
        sync=true;
        auto owned=[&]{PresentationEffects effects(services);Check(!effects.HasExitEvents());
            Check(!effects.RegisterExit({},"none",{}));return effects.CreateRequest("sample: ");}();
        auto& request=*owned;Check(roots.empty() && submits==0);request.Start(&entity,&path);
        Check(request.Status().submitted && request.Status().completions==1 && request.Status().loaded_completions==1);
        Check(removes==0 && releases==0 && !request.Status().closed);
        // A replacement global manager must not steal the old request's cleanup.
        current_manager=&other_manager;request.Cancel();request.Service();
        Check(removes==1 && last_removed_manager==&manager && !request.Status().closed && releases==0);
        request.Service();Check(removes==1);
        first.released=true;request.Service();Check(request.Status().closed && roots.empty());
        Invoke(first,&effect);Check(request.Status().loaded_completions==1); // disconnected late callback
        Check(!traces.empty());for(const auto& line:traces)Check(line.rfind("sample: ",0)==0);
    }
    Reset();
    {
        OwnedEffectRequest request(Services());request.Start(&entity,&path);request.Cancel();
        remove_match=false;request.Service();Check(removes==1 && !request.Status().closed);
        request.Service();request.Service();Check(removes==1 && releases==0);
        // Loading finished after unmatched cancellation. Record only in callback;
        // the next owner service performs integrated removal, with no adoption.
        Invoke(first,&effect);first.state=3;Check(removes==1);
        remove_match=true;release_on_remove=true;request.Service();
        Check(removes==2 && request.Status().closed && request.Status().completions==2 && releases==0);
    }
    Reset();
    {
        OwnedEffectRequest request(Services());request.Start(&entity,&path);request.Cancel();
        ++first.id;request.Service();Check(request.Status().closed && removes==0 && releases==0);
    }
    Reset();
    {
        OwnedEffectRequest request(Services());request.Start(&entity,&path);request.Cancel();
        id_fault=true;remove_match=false;request.Service();
        Check(!request.Status().closed && removes==1); // failed observation does not block official cancellation
        id_fault=false;first.released=true;request.Service();Check(request.Status().closed);
    }
    Reset();
    {
        configure_fault=true;OwnedEffectRequest request(Services());request.Start(&entity,&path);
        Check(!request.Status().submitted && request.Status().closed && submits==0 && releases==1 && removes==0);
    }
    Reset();
    {
        submit_fault=true;OwnedEffectRequest request(Services());request.Start(&entity,&path);
        Check(request.Status().submitted && request.Status().cancelled && request.Status().submit.exception==0xE0680001);
        release_on_remove=true;request.Service();Check(request.Status().closed && removes==1 && releases==0);
    }
    Reset();
    {
        OwnedEffectRequest a(Services());a.Start(&entity,&path);
        acquiring=&second;OwnedEffectRequest b(Services());b.Start(&entity,&path);
        Invoke(first,&effect,&second);a.Service();Check(a.Status().foreign_completions==1 && a.Status().completions==0);
        a.Cancel();release_on_remove=true;a.Service();Check(a.Status().closed && !b.Status().cancelled && !second.released);
        b.Cancel();b.Service();Check(b.Status().closed && roots.empty());
    }
    Reset();
    {
        OwnedEffectRequest request(Services());request.Cancel();request.Start(&entity,&path);
        Check(request.Status().closed && submits==0 && roots.empty());
    }
    Reset();
    {
        auto services=Services();bool observed=false,retired=false;int observations=0,retirements=0;
        services.observe_request=[&](EffectReference ref,void* owner){
            Check(submits==0 && owner==&entity && ref.parameters==&first && !retired);
            observed=true;++observations;
        };
        services.retire_request=[&](EffectReference ref){
            Check(observed && first.released && ref.parameters==&first);retired=true;++retirements;
        };
        sync=true;OwnedEffectRequest request(services);request.Start(&entity,&path);
        Check(observed && !retired && observations==1 && request.Status().completions==1);
        request.Cancel();request.Service();Check(!retired);
        first.released=true;request.Service();request.Service();
        Check(retired && retirements==1 && request.Status().closed);
    }
    Reset();
    {
        auto services=Services();bool retired=false;
        services.observe_request=[](EffectReference,void*){throw std::runtime_error("partial source observation");};
        services.retire_request=[&](EffectReference){retired=true;throw std::runtime_error("retained weak cleanup receipt");};
        OwnedEffectRequest request(services);request.Start(&entity,&path);
        Check(submits==0 && releases==1 && retired && request.Status().closed && roots.empty());
    }
    Reset();
    std::cout<<"Owned effects: synchronous/late callbacks, original manager, pool reuse, partial writes/submission, cancellation and two-request isolation passed; no game acceptance.\n";
}
