#include <hksr/entity/EntityProvenance.hpp>
#include <hksr/presentation/PresentationSources.hpp>
#include "../../src/runtime/RepeatedDiagnostics.hpp"
#include <Windows.h>
#include <map>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Handle {void* object;int kind;};
std::map<int,Handle> handles;
int sequence=0,frees=0;bool fail_get=false,fail_free=false,fail_create=false;
void Check(bool ok){if(!ok)throw std::runtime_error("entity provenance test failed");}
int __fastcall Keep(void* object,int previous,int kind,void*) {
    // Mirrors the observed IL2CPP unsupported resurrection branch. A test
    // fixture accepting every enum value hid the real incompatibility.
    if(kind==1)RaiseException(0xe0760504,0,0,nullptr);
    Check(!previous && (kind==0 || kind==2));
    if(fail_create)RaiseException(0xe0760501,0,0,nullptr);
    handles[++sequence]={object,kind};return sequence;
}
void* __fastcall Target(int handle,void*) {
    if(fail_get)RaiseException(0xe0760502,0,0,nullptr);
    return handles.at(handle).object;
}
void __fastcall Free(int handle,void*) {
    ++frees;if(fail_free)RaiseException(0xe0760503,0,0,nullptr);
    handles.erase(handle);
}
void Collect(void* object) {
    for(const auto& h:handles)Check(h.second.object!=object || h.second.kind==0);
    for(auto& h:handles)if(h.second.object==object)h.second.object=nullptr;
}
template<class F>bool Throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
}
void TestEntityProvenance() {
    ReferenceMethods refs{{reinterpret_cast<void*>(&Keep),nullptr},{reinterpret_cast<void*>(&Target),nullptr},{reinterpret_cast<void*>(&Free),nullptr},{},{}};
    EntityProvenance provenance(refs);EntityLifecycle live;
    int entity,world,owner,other;
    auto* e=reinterpret_cast<GameEntity*>(&entity);auto* w=reinterpret_cast<GameWorld*>(&world);
    EntityToken token;int births=0;
    {
        EntityCreationScope scope(w,&owner,[&](EntityOrigin value){++births;provenance.Remember(value);});
        EntityCreationScope::FactoryCompleted(w,e);
        token=EntityCreationScope::OwnedToken(e,&owner);
        Check(provenance.Find(e).token.generation==token.generation);
        EntityCreationScope::FactoryCompleted(w,e);Check(births==1 && handles.size()==1);
        Check(live.Track(e,&owner).generation==token.generation);
        EntityCreationScope::DisposalCompleted(e);
        Check(!EntityCreationScope::OwnedToken(e,&owner).entity);
        Check(provenance.Find(e).token.generation==token.generation);
    }
    live.Disposed(token);live.Forget(token);
    Check(!live.OwnedToken(e,&owner).entity && provenance.Find(e).token.generation==token.generation);
    Check(provenance.Find(e).world==w); // no dereference or retained world handle
    Check(provenance.Contains({token,w}));
    Check(!provenance.Contains({{e,token.generation+1},w}));
    for(const auto& h:handles)Check(h.second.kind==0 && h.second.object==e);
    fail_get=true;Check(Throws([&]{provenance.Find(e);}));fail_get=false;
    Check(provenance.Find(e).token.generation==token.generation);
    Collect(e);Check(!provenance.Find(e).token.entity && handles.empty());
    // Same native address after managed collection is NOT the former actor.
    Check(!provenance.Find(e).token.entity);
    const auto next=live.Track(e,&owner);provenance.Remember({next,w});
    Check(next.generation!=token.generation && provenance.Find(e).token.generation==next.generation);
    provenance.Remember({next,w});Check(handles.size()==1);
    Check(Throws([&]{provenance.Remember({next,reinterpret_cast<GameWorld*>(&other)});}));
    // Entire creation may be disposed/throw before outer return; the inner
    // birth receipt remains attributable even though it was never adopted.
    auto* partial=reinterpret_cast<GameEntity*>(&other);
    {
        EntityCreationScope scope(w,&owner,[&](EntityOrigin value){provenance.Remember(value);});
        EntityCreationScope::FactoryCompleted(w,partial);EntityCreationScope::DisposalCompleted(partial);
        Check(provenance.Find(partial).token.entity==partial && !scope.Receipt().Unique());
    }
    Collect(partial);provenance.Collect();Check(!provenance.Find(partial).token.entity);
    // Failed observation is unknown, not an established official source.
    fail_create=true;Check(Throws([&]{provenance.Remember({{partial,999},w});}));fail_create=false;
    Check(Throws([&]{provenance.Find(partial);}));
    Check(!provenance.Close()); // uncertain allocation receipt retained
    Check(Throws([&]{provenance.Find(e);}));live.Disposed(next);live.Forget(next);
    // Free is never retried after an uncertain result, even by repeated Close.
    EntityProvenance cleanup(refs);cleanup.Remember({{e,1000},w});
    fail_free=true;Check(!cleanup.Close());const auto once=frees;fail_free=false;
    Check(!cleanup.Close() && frees==once);
    handles.clear(); // synthetic runtime fixture cleanup, not real GC handles
    EntityProvenance success(refs);success.Remember({{e,1001},w});Collect(e);success.Collect();
    Check(success.Close() && handles.empty());
    {
        detail::RepeatedDiagnostics diagnostic;
        Check(diagnostic.Publish("Effect scope: source unavailable"));
        Check(!diagnostic.Publish("Effect scope: source unavailable"));
        Check(diagnostic.Publish("Effect scope: another read failed"));
        Check(diagnostic.Publish("borrow observed") && diagnostic.Publish("borrow observed"));
        Check(diagnostic.Suppressed()==1 && diagnostic.Count("Effect scope: source unavailable")==2);
        PresentationSources sources(refs);int foreign;
        fail_create=true;Check(Throws([&]{sources.Remember({{e,1999},w});}));fail_create=false;
        Check(Throws([&]{sources.Identify(e);})); // THIS failed source remains unknown
        Check(!sources.Identify(reinterpret_cast<GameEntity*>(&foreign)).token.entity);
        sources.Remember({{reinterpret_cast<GameEntity*>(&other),2000},w});
        Check(sources.Identify(reinterpret_cast<GameEntity*>(&other)).token.generation==2000);
        Collect(&other);Check(Throws([&]{sources.Collect();}));
        Check(!sources.Identify(reinterpret_cast<GameEntity*>(&other)).token.entity);
        Check(!sources.Close());Check(handles.empty());
    }
    {
        PresentationSources sources(refs);int params,public_entity;
        const EntityOrigin origin{{e,2001},w};sources.Remember(origin);
        EffectReference ref{0,0,&params}; // zero is a valid official request ID
        EffectRelationsSnapshot facts;facts.same_generation=true;
        facts.id_before.read={true,0};facts.id_after.read={true,0};
        facts.caster={{true,0},nullptr};facts.target={{true,0},e};
        Check(Throws([&]{sources.Identify(ref,facts);})); // target is NOT caster
        sources.RememberRequest(ref,e);sources.RememberRequest(ref,e);
        Check(sources.Identify(ref,facts).token.generation==2001 && handles.size()==2);
        // Retiring the entity's live owner cannot erase a pending request source.
        Collect(e);sources.Collect();Check(sources.HasSource(origin));
        Check(sources.Identify(ref,facts).token.generation==2001);
        sources.RetireRequest({1,0,&params});Check(sources.HasSource(origin));
        sources.RetireRequest(ref);Check(!sources.HasSource(origin));
        facts.caster={{true,0},&public_entity};
        Check(!sources.Identify(ref,facts).token.entity);
        sources.Remember({{e,2002},w});sources.RememberRequest(ref,e);
        ++ref.id;facts.id_before.value=facts.id_after.value=1;
        // Pool reuse cannot inherit the old explicit source, even at same address.
        Check(!sources.Identify(ref,facts).token.entity);
        sources.RememberRequest(ref,e);
        sources.RetireRequest({0,0,&params});
        Check(sources.Identify(ref,facts).token.generation==2002);
        facts.same_generation=false;Check(Throws([&]{sources.Identify(ref,facts);}));
        facts.same_generation=true;facts.id_after.value=2;Check(Throws([&]{sources.Identify(ref,facts);}));
        facts.id_after.value=1;
        fail_get=true;Check(Throws([&]{sources.Identify(ref,facts);}));fail_get=false;
        Collect(&params);sources.Collect();Check(handles.size()==1);
        sources.RetireRequest(ref);Check(sources.Close() && handles.empty());
        Check(Throws([&]{sources.Identify(e);}));
    }
    std::cout<<"Entity provenance passed: birth/disposal separation, weak identity, collection/reuse, partial creation and no-retry failures. Local fixtures.\n";
}
