#include <hksr/unity/AssetLeases.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
int asset, path, type, metadata;
int loads, drops, roots, frees, gets;
bool target_fault=false, drop_fault=false, free_fault=false, load_fault=false;
void Check(bool ok){if(!ok)throw std::runtime_error("asset lease test failed");}
void* __fastcall Load(void* p,void* t,bool option,void* info){
    Check(p==&path && t==&type && !option && info==&metadata);++loads;
    if(load_fault)RaiseException(0xE0420001,0,0,nullptr);
    return &asset;
}
int __fastcall Retain(void* object,int handle,int kind,void*){Check(object==&asset && !handle && kind==2);return ++roots;}
void* __fastcall Get(int handle,void*){
    Check(handle>0);++gets;
    if(target_fault)RaiseException(0xE0420002,0,0,nullptr);
    return &asset;
}
void __fastcall Drop(void* object,void*){
    Check(object==&asset);++drops;
    if(drop_fault)RaiseException(0xE0420003,0,0,nullptr);
}
void __fastcall Free(int handle,void*){
    Check(handle>0);++frees;
    if(free_fault)RaiseException(0xE0420004,0,0,nullptr);
}
template<class T> Method M(T function){return {reinterpret_cast<void*>(function),&metadata};}
}
void TestAssetLeases(){
    AssetLeases leases({M(Retain),M(Get),M(Free),M(Load),M(Drop)});
    const auto a=leases.Acquire(&path,&type,false), b=leases.Acquire(&path,&type,false);
    Check(a!=b && leases.Object(a)==&asset && leases.Object(b)==&asset && loads==2 && roots==2);
    Check(leases.Release(a).complete && drops==1 && frees==1 && leases.Object(b)==&asset);
    Check(leases.Release(a).complete && drops==1 && frees==1);
    Check(leases.Release(b).complete && drops==2 && frees==2);
    leases.Forget(a);leases.Forget(b);
    target_fault=true;
    const auto c=leases.Acquire(&path,&type,false);
    Check(!leases.Object(c) && leases.Status(c).complete && drops==3 && frees==3);
    Check(leases.Status(c).target.call.exception==0xE0420002);
    leases.Forget(c);target_fault=false;
    const auto d=leases.Acquire(&path,&type,false);
    drop_fault=true;
    auto result=leases.Release(d);
    Check(result.asset_release==ReleaseState::OutcomeUnknown && !result.complete && frees==3);
    const auto before=drops;drop_fault=false;
    leases.Release(d);Check(drops==before && frees==3);
    bool refused=false;try{leases.Forget(d);}catch(const std::logic_error&){refused=true;}Check(refused);
    const auto e=leases.Acquire(&path,&type,false);free_fault=true;
    result=leases.Release(e);
    Check(result.asset_release==ReleaseState::Returned && result.root_release==ReleaseState::OutcomeUnknown && !result.complete);
    const auto free_before=frees;free_fault=false;
    leases.Release(e);Check(frees==free_before);
    load_fault=true;
    const auto f=leases.Acquire(&path,&type,false);
    Check(!leases.Status(f).complete && !leases.Object(f));
    const auto drops_before=drops;leases.Release(f);Check(drops==drops_before);load_fault=false;
    const auto calls_before=loads;
    AssetLeases missing({M(Retain),M(Get),{},M(Load),M(Drop)});
    refused=false;try{missing.Acquire(&path,&type,false);}catch(const std::runtime_error&){refused=true;}
    Check(refused && loads==calls_before);
    std::cout<<"Asset lease tests passed: independent acquisitions, explicit and non-repeated cleanup.\n";
}
