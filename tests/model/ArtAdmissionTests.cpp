#include <hksr/model/ArtAdmission.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <cstddef>
#include <iostream>
#include <stdexcept>
namespace {
using namespace hksr::api;
int info,key,path,art,callback,points,source,parent,component,game_object;
struct Model { int unrelated[12]; void* logical_path=&path; } model;
bool failure=false,active_value=false,clone_world=false;
void Check(bool ok) { if(!ok) throw std::runtime_error("Art admission contract mismatch"); }
template<class T> Method M(T f) { return {reinterpret_cast<void*>(f),&info}; }
void __fastcall Supply(void* self,void* slot,void* logical,bool option,void* done,void* supplied,void* metadata) {
    Check(self==&model && slot==&key && logical==&path && !option && done==&callback && supplied==&art && metadata==&info);
    if(failure) RaiseException(0xE0620001,0,0,nullptr);
}
void __fastcall Unload(void* self,bool release,void* metadata) { Check(self==&model && release && metadata==&info); }
void __fastcall Hide(void* self,void* names,void* metadata) { Check(self==&model && names==&points && metadata==&info); }
void __fastcall Initialize(void* self,void* metadata) { Check(self==&game_object && metadata==&info); }
void __fastcall SetActive(void* self,bool active,void* metadata) { Check(self==&game_object && metadata==&info); active_value=active; }
void* __fastcall Clone(void* original,void* transform,bool world,void* metadata) {
    Check(original==&source && transform==&parent && metadata==&info); clone_world=world; return &art;
}
void* __fastcall GameObject(void* value,void* metadata) { Check(value==&component && metadata==&info); return &game_object; }
int __fastcall ChildCount(void* value,void* metadata) { Check(value==&parent && metadata==&info); return 3; }
DeclaredMethod D(const char* name,const char* result,std::initializer_list<const char*> args,std::uint32_t flags,Method m) {
    DeclaredMethod out; out.name=name; out.result=result; for(auto p:args)out.parameters.push_back(p); out.is_static=(flags&0x10)!=0; out.entry=m; return out;
}
}
void TestArtAdmission() {
    // Signatures copied from retained declarations; pointers/field offset here
    // are substitutes. This does not test Unity's loading or ownership behavior.
    DeclaredType model_type{"RPG.GameCore","CharacterModelComponent",{
        D("LoadSubUnityGO","System.Void",{"System.String","System.String","System.Boolean","System.Action","UnityEngine.GameObject"},134,M(Supply)),
        D("OnArtNodeUnload","System.Void",{"System.Boolean"},129,M(Unload)),
        D("_HideWeapons","System.Void",{"System.String[]"},129,M(Hide))}};
    DeclaredType go{"UnityEngine","GameObject",{
        D(".ctor","System.Void",{},6278,M(Initialize)),
        D(".ctor","System.Void",{"System.String"},6278,{}),
        D("SetActive","System.Void",{"System.Boolean"},134,M(SetActive))}};
    DeclaredType object{"UnityEngine","Object",{
        D("Instantiate","UnityEngine.Object",{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},150,M(Clone)),
        D("Instantiate","T",{"T","UnityEngine.Transform","System.Boolean"},150,{})}};
    DeclaredType comp{"UnityEngine","Component",{D("get_gameObject","UnityEngine.GameObject",{},2182,M(GameObject))}};
    DeclaredType transform{"UnityEngine","Transform",{D("get_childCount","System.Int32",{},2182,M(ChildCount))}};
    MetadataResolver resolver({model_type,go,object,comp,transform});
    std::vector<DeclaredField> fields{{"RPG.GameCore","CharacterModelComponent","_ArtAssetPath","System.String",offsetof(Model,logical_path)}};
    const auto methods=ResolveArtAdmissionMethods(resolver,fields); Check(methods.binding_errors.empty());
    ArtAdmission admission(methods); auto* model_ptr=reinterpret_cast<CharacterModelComponent*>(&model);
    const auto logical=admission.LogicalPath(model_ptr); Check(logical.call.returned && logical.object==&path);
    Check(admission.Supply(model_ptr,&key,logical.object,false,&callback,&art).returned);
    failure=true; const auto failed=admission.Supply(model_ptr,&key,&path,false,&callback,&art); failure=false;
    Check(!failed.returned && failed.exception==0xE0620001);
    Check(admission.Unload(model_ptr,true).returned && admission.HideWeapons(model_ptr,&points).returned);
    fields.push_back(fields.front()); const auto duplicate=ResolveArtAdmissionMethods(resolver,fields);
    Check(!duplicate.has_logical_path && duplicate.binding_errors.size()==1 && duplicate.unload.code);
    Check(!ArtAdmission(duplicate).LogicalPath(model_ptr).call.returned);
    const auto visual_methods=ResolveVisualObjectMethods(resolver); Check(visual_methods.binding_errors.empty());
    VisualObjects visuals(visual_methods);
    Check(visuals.Initialize(&game_object).returned && visuals.SetActive(&game_object,true).returned && active_value);
    Check(visuals.SetActive(&game_object,false).returned && !active_value);
    Check(visuals.CloneUnderParent(&source,&parent,false).object==&art && !clone_world);
    Check(visuals.CloneUnderParent(&source,&parent,true).object==&art && clone_world);
    Check(visuals.GameObject(&component).object==&game_object);
    Check(visuals.ChildCount(&parent).read.returned && visuals.ChildCount(&parent).value==3);
    std::cout<<"Art admission calls passed: separate logical slot, declared field offset, parented clone and partial call failures; no game adoption proof.\n";
}
