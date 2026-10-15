#include <hksr/animation/ControllerBinding.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<hksr::api::ControllerBinding>);
static_assert(!std::is_copy_assignable_v<hksr::api::ControllerBinding>);

namespace {
using namespace hksr;
int entity,world,info,model_type,prefab_type,animator_type,prefab,source_animator,controller,manager,array;
struct Loader { void* model=nullptr; void* manager=nullptr; } loader;
struct Model { void* loader=nullptr; } model,other_model;
auto* owner=reinterpret_cast<api::GameEntity*>(&entity);
auto* own_world=reinterpret_cast<api::GameWorld*>(&world);
void* current_model=&model;
std::map<int,void*> roots;
std::vector<std::unique_ptr<std::string>> strings;
int next=0,count=1;
unsigned loads=0,releases=0,setup=0,applies=0,removes=0,reads=0;
bool installed=false,path_installed=false,apply_fault=false,remove_fault=false,dispose_on_apply=false,free_fault=false;
std::string installed_tag;
api::ControllerBinding* active=nullptr;
api::WorldFrame frame;
bool close_on_manager=false;
void Check(bool value) { if(!value) throw std::runtime_error("controller binding integration failed"); }
template<class T> api::Method M(T f) { return {reinterpret_cast<void*>(f),&info}; }
int __fastcall Retain(void* value,int,int,void*) { roots[++next]=value; return next; }
void* __fastcall Target(int id,void*) { return roots.at(id); }
void __fastcall Free(int id,void*) { if(free_fault) RaiseException(0xE0690003,0,0,nullptr); roots.erase(id); }
void* __fastcall Load(std::string* path,void* type,bool option,void*) {
    Check(*path=="Character/source.prefab" && type==&prefab_type && !option); ++loads; return &prefab;
}
void __fastcall Release(void* value,void*) { Check(value==&prefab && !installed); ++releases; }
std::uint32_t __fastcall Id(void*,void*) { ++reads; return 1; }
api::GameWorld* __fastcall World(void* value,void*) { Check(value==owner); ++reads; return own_world; }
bool __fastcall Loaded(void*,void*) { return true; }
void* __fastcall Component(void* value,void* type,void*) { Check(value==owner && type==&model_type); ++reads; return current_model; }
void* __fastcall Components(void* value,void* type,bool hidden,void*) {
    Check(value==&prefab && type==&animator_type && hidden); return &array;
}
int __fastcall Length(void* value,void*) { Check(value==&array); return count; }
void* __fastcall Get(void* value,int index,void*) { Check(value==&array && index==0); return &source_animator; }
void* __fastcall Controller(void* value,void*) { Check(value==&source_animator); return &controller; }
void __fastcall Path(void* value,std::string* path,void* key,void*) {
    Check(value==&model && !key);
    if(path) { Check(*path=="Controller/path"); path_installed=true; ++applies; }
    else { if(path_installed) { ++removes; path_installed=false; } else ++setup; model.loader=&loader; loader.model=&model; }
}
void* __fastcall Manager(void* value,void*) {
    Check(value==&loader); loader.manager=&manager;
    if(close_on_manager) Check(!active->Close(frame).complete);
    return &manager;
}
void __fastcall Replace(void* value,void* asset,std::string* tag,void*) {
    Check(value==&manager && tag->find("Hksr/Controller/")==0);
    if(asset) {
        Check(asset==&controller && !installed); installed=true; installed_tag=*tag; ++applies;
        if(dispose_on_apply) { api::TrackedEntities().Disposed(api::TrackedEntities().Current(owner)); installed=false; }
        if(apply_fault) RaiseException(0xE0690001,0,0,nullptr);
    } else {
        Check(*tag==installed_tag); ++removes;
        if(remove_fault) RaiseException(0xE0690002,0,0,nullptr);
        installed=false;
    }
}
struct Objects:api::ManagedObjects {
    api::ObjectResult ManagedType(const char*,const char* name) override {
        const std::string n=name;
        if(n=="CharacterModelComponent") return {{true,0},&model_type};
        if(n=="GameObject") return {{true,0},&prefab_type};
        Check(n=="Animator"); return {{true,0},&animator_type};
    }
    api::ObjectResult NewStringUtf8(const char* value) override {
        strings.push_back(std::make_unique<std::string>(value)); return {{true,0},strings.back().get()};
    }
    api::ObjectResult AllocateObject(const char*,const char*) override { throw std::runtime_error("no cloning/allocation expected"); }
};
}
void TestControllerBinding() {
    const api::ReferenceMethods refs{M(Retain),M(Target),M(Free),M(Load),M(Release)};
    api::AnimationControlMethods methods; methods.controller=M(Controller); methods.replace_path=M(Path);
    methods.override_manager=M(Manager); methods.replace_object=M(Replace);
    const api::EntityAccess entities({M(Id),M(World),M(Loaded),M(Component),{},{},{},{}});
    const api::ControllerBindingServices services{api::AnimationControl(methods),entities,
        api::SceneObjects({{},M(Components),{},{},{}}),api::ManagedArrays({{},M(Length),M(Get),{},{}}),
        {offsetof(Model,loader),true,{}},{offsetof(Loader,model),true,{}},{offsetof(Loader,manager),true,{}}};
    const api::ControllerSource source{api::ControllerSourceKind::PrefabAnimator,"Character/source.prefab"};
    Objects objects; frame.world=own_world; frame.disposing={false,{true,0}};
    auto foreign=frame; foreign.world=reinterpret_cast<api::GameWorld*>(1);
    auto token=api::TrackedEntities().Track(owner);
    {
        api::ControllerBinding binding(token,own_world,refs,services); active=&binding;
        Check(!binding.Apply(foreign,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete && loads==0);
        Check(binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        Check(installed && applies==1 && releases==0 && binding.Status().source_lease.ready);
        Check(!binding.Close(foreign).complete && removes==0);
        Check(binding.Close(frame).complete && !installed && releases==1 && roots.empty());
        Check(binding.Close(frame).complete && removes==1);
    }
    {
        count=2; auto writes=applies;
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(!binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        Check(binding.Close(frame).complete && writes==applies && roots.empty()); count=1;
    }
    {
        close_on_manager=true;
        api::ControllerBinding binding(token,own_world,refs,services); active=&binding;
        Check(!binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        Check(!binding.Status().submitted && binding.Close(frame).complete && roots.empty()); close_on_manager=false;
    }
    {
        apply_fault=true;
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(!binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        Check(installed && binding.Status().apply.exception==0xE0690001);
        apply_fault=false; Check(binding.Close(frame).complete && !installed && roots.empty());
    }
    {
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,
            {api::ControllerSourceKind::ResourcePath,"Controller/path"}).complete);
        Check(path_installed && binding.Status().lease==0);
        Check(binding.Close(frame).complete && !path_installed && roots.empty());
    }
    {
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        auto cleared=removes, released=releases;
        current_model=&other_model;
        Check(!binding.Close(frame).complete && cleared==removes && released==releases && !binding.Status().detached);
        api::TrackedEntities().Disposed(token); installed=false; const auto before=reads;
        Check(binding.Close(foreign).complete && reads==before && roots.empty()); current_model=&model;
    }
    api::TrackedEntities().Forget(token); token=api::TrackedEntities().Track(owner);
    {
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        remove_fault=true; Check(!binding.Close(frame).complete); const auto before=removes;
        remove_fault=false; Check(!binding.Close(frame).complete && before==removes); // don't replay an uncertain mutation
        api::TrackedEntities().Disposed(token); installed=false;
        Check(binding.Close(foreign).complete && roots.empty());
    }
    api::TrackedEntities().Forget(token); token=api::TrackedEntities().Track(owner);
    {
        dispose_on_apply=true; api::ControllerBinding binding(token,own_world,refs,services);
        Check(!binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        Check(binding.Close(foreign).complete && roots.empty()); dispose_on_apply=false;
    }
    api::TrackedEntities().Forget(token); token=api::TrackedEntities().Track(owner);
    {
        api::ControllerBinding binding(token,own_world,refs,services);
        Check(binding.Apply(frame,reinterpret_cast<api::CharacterModelComponent*>(&model),objects,source).complete);
        free_fault=true; const auto before=releases;
        Check(!binding.Close(frame).complete && binding.Status().detached && releases==before+1 && !installed);
        free_fault=false; Check(!binding.Close(frame).complete && releases==before+1);
        // Outcome-unknown handles remain receipts, not an excuse to block the
        // already detached model's next binding. Substitute store dies below.
    }
    api::TrackedEntities().Disposed(token); api::TrackedEntities().Forget(token); roots.clear();
    std::cout<<"Controller binding passed: independent source lease, scoped tag, partial failure, cancellation, owned model checks, path mode, removal/GC separation; local only.\n";
}
