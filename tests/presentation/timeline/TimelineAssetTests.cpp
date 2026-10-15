#include <hksr/presentation/timeline/TimelineAsset.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <Windows.h>
namespace {
using namespace hksr::api;
void Check(bool v){if(!v)throw std::runtime_error("Timeline asset test failed");}
struct Track {const char* space;const char* name;bool muted;int markers;};
Track light{"RPGTools.Timeline","LightControlTrack",false,0},unknown{"Test","LightDecoration",true,0};
std::vector<Track*> tracks{&light,&unknown};
std::map<int,void*> handles;int next=0,info,asset,type_token,loads=0,releases=0;
bool missing=false,fail_flags=false;
std::vector<std::unique_ptr<std::string>> strings;
template<class T> Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
int __fastcall Retain(void* p,int,int,void*){handles[++next]=p;return next;}
void* __fastcall Target(int id,void*){return handles.at(id);}
void __fastcall Free(int id,void*){Check(handles.erase(id)==1);}
void* __fastcall Load(std::string* p,void* t,bool option,void*){Check(*p=="resource.playable" && t==&type_token && !option);++loads;return missing?nullptr:&asset;}
void __fastcall Release(void* p,void*){Check(p==&asset);++releases;}
void* __fastcall Flat(void* p,void*){Check(p==&asset && handles.size()>=3);return &tracks;}
int __fastcall Count(void* p,void*){Check(p==&tracks);return static_cast<int>(tracks.size());}
void* __fastcall Item(void* p,int i,void*){Check(p==&tracks);return tracks.at(i);}
bool __fastcall Muted(Track* t,void*){if(fail_flags)RaiseException(0xE041CAFE,0,0,nullptr);return t->muted;}
int __fastcall Markers(Track* t,void*){return t->markers;}
struct Fixture:MethodResolver,ManagedObjects,ObjectMetadata {
    Method Resolve(const char*,const char*,const char* n,const char*,std::initializer_list<const char*>,MethodKind)override {
        const std::string name=n;
        if(name=="get_flattenedTracks")return M(Flat);
        if(name=="get_mutedInHierarchy")return M(Muted);
        if(name=="GetMarkerCount")return M(Markers);
        throw std::runtime_error("unexpected Timeline method");
    }
    ObjectResult NewStringUtf8(const char* p)override {strings.push_back(std::make_unique<std::string>(p));return {{true,0},strings.back().get()};}
    ObjectResult ManagedType(const char* ns,const char* t)override {Check(std::string(ns)=="UnityEngine.Timeline" && std::string(t)=="TimelineAsset");return {{true,0},&type_token};}
    ObjectResult AllocateObject(const char*,const char*)override {throw std::runtime_error("resource inspector must not create objects");}
    DeclaredType ReadObjectType(void* p)override {
        if(p==&asset)return {"UnityEngine.Timeline","TimelineAsset",{}};
        if(p==&tracks)return {"System.Collections.Generic","List`1",{
            {"get_Count","System.Int32",{},false,M(Count)},
            {"get_Item","UnityEngine.Timeline.TrackAsset",{"System.Int32"},false,M(Item)}}};
        auto* t=static_cast<Track*>(p);return {t->space,t->name,{}};
    }
    std::vector<DeclaredField> ReadObjectFields(void*)override {throw std::runtime_error("unexpected fields");}
};
}
void TestTimelineAsset() {
    Fixture fixture;
    TimelineAssetReader reader(fixture,fixture,fixture,{M(Retain),M(Target),M(Free),M(Load),M(Release)});
    auto result=reader.Inspect("resource.playable");
    Check(result.complete && result.tracks.size()==2 && handles.empty() && loads==1 && releases==1);
    Check(!result.tracks[0].muted && result.tracks[1].muted);
    unknown.muted=false;result=reader.Inspect("resource.playable");
    Check(result.complete && !result.tracks[1].muted && handles.empty());
    unknown.muted=true;light.markers=1;result=reader.Inspect("resource.playable");
    Check(result.complete && result.tracks[0].markers==1);light.markers=0;
    fail_flags=true;result=reader.Inspect("resource.playable");
    Check(!result.complete && !result.error.empty() && handles.empty());fail_flags=false;
    missing=true;result=reader.Inspect("resource.playable");
    Check(!result.complete && handles.empty() && loads==5 && releases==4);missing=false;
    tracks.clear();result=reader.Inspect("resource.playable");Check(result.complete && result.tracks.empty() && handles.empty());
    std::cout<<"Timeline resource reader passed: no object creation, leases/roots released, flags/markers, empty resource and native read failure. Synthetic engine only.\n";
}
