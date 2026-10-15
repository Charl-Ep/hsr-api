#include <hksr/control/LookSupport.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <type_traits>
#include <stdexcept>

namespace {
using namespace hksr;
using Values=std::vector<void*>;
int info,normal_type,alternate_type,transform_type,go_type,sequence=0;
struct Obj { int id=0; std::wstring name; Obj* parent=nullptr; bool alive=true; std::wstring json; };
Obj source{1,L"Npc"},source_head{2,L"头",&source},source_look{3,L"Look",&source};
Obj art{11,L"Art"},head{12,L"头",&art},look{13,L"Look",&art};
Obj unrelated{22,L"unused",&art},duplicate{23,L"unused",&art};
std::vector<Obj*> private_look;
std::vector<Obj*> private_nodes;
std::vector<std::unique_ptr<Values>> arrays;
std::vector<std::unique_ptr<std::wstring>> strings;
std::map<int,void*> handles;
unsigned acquire=0,release=0,add=0,overwrite=0,destroy=0;
bool fail_add=false,fail_write=false,wrong_reference=false;
void Check(bool value) { if(!value) throw std::runtime_error("LookAt support test failed"); }
template<class T> api::Method M(T f) { return {reinterpret_cast<void*>(f),&info}; }
api::ObjectResult Ref(void* p) { return {{true,0},p}; }
std::wstring W(const std::string& text) {
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    Check(size>0); std::wstring out(static_cast<std::size_t>(size),0);
    Check(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),size)==size); return out;
}
int __fastcall Keep(void* p,int,int,void*) { handles[++sequence]=p; return sequence; }
void* __fastcall Target(int id,void*) { return handles.at(id); }
void __fastcall Free(int id,void*) { handles.erase(id); }
void* __fastcall Load(void* path,void* type,bool option,void*) {
    Check(*static_cast<std::wstring*>(path)==L"npc.prefab" && type==&go_type && !option); ++acquire; return &source;
}
void __fastcall Release(void* value,void*) { Check(value==&source); ++release; }
bool __fastcall Alive(Obj* value,void*) { return value->alive; }
void __fastcall Destroy(Obj* value,void*) { Check(value==&look); ++destroy; } // deferred, not immediately dead
void* __fastcall Transform(Obj* value,void*) { return value; }
void* __fastcall Parent(Obj* value,void*) { return value->parent; }
void* __fastcall Name(Obj* value,void*) { return &value->name; }
int __fastcall InstanceId(Obj* value,void*) { return value->id; }
void* __fastcall ComponentTransform(Obj* value,void*) { Check(value==&source_look); return &source; }
void* __fastcall GameObject(Obj* value,void*) { return value; }
void* __fastcall Components(Obj* root,void* type,bool inactive,void*) {
    Check(inactive && (root==&source || root==&art));
    auto values=std::make_unique<Values>();
    if(type==&normal_type) {
        if(root==&source) values->push_back(&source_look);
        else for(auto* object:private_look) if(object->alive) values->push_back(object);
    } else if(type==&transform_type) {
        if(root==&source) *values={&source,&source_head};
        else for(auto* node:private_nodes) values->push_back(node);
    } else Check(type==&alternate_type);
    auto* out=values.get(); arrays.push_back(std::move(values)); return out;
}
int __fastcall Length(Values* values,void*) { return static_cast<int>(values->size()); }
void* __fastcall Item(Values* values,int index,void*) { return values->at(index); }
void* __fastcall Type(void* object,void*) { Check(object==&source_look); return &normal_type; }
void* __fastcall Serialize(Obj* object,bool pretty,void*) { Check(!pretty); return &object->json; }
void* __fastcall Add(void* owner,void* type,void*) {
    Check(owner==&art && type==&normal_type); ++add; private_look.push_back(&look);
    if(fail_add) RaiseException(0xE0690001,0,0,nullptr);
    return &look;
}
void __fastcall Overwrite(std::wstring* text,Obj* object,void*) {
    Check(object==&look); ++overwrite; object->json=*text;
    if(wrong_reference) object->json=source_look.json;
    if(fail_write) RaiseException(0xE0690002,0,0,nullptr);
}
int __fastcall StringLength(std::wstring* text,void*) { return static_cast<int>(text->size()); }
std::uint16_t __fastcall Character(std::wstring* text,int index,void*) { return static_cast<std::uint16_t>(text->at(index)); }
struct Objects:api::ManagedObjects {
    api::ObjectResult NewStringUtf8(const char* value) override {
        strings.push_back(std::make_unique<std::wstring>(W(value))); return Ref(strings.back().get());
    }
    api::ObjectResult ManagedType(const char* ns,const char* type) override {
        const std::string name=type;
        if(name=="LookAtIK") { Check(std::string(ns)=="RPG.Client"); return Ref(&normal_type); }
        if(name=="ZatzkaLookAtIK") return Ref(&alternate_type);
        if(name=="Transform") return Ref(&transform_type);
        Check(name=="GameObject"); return Ref(&go_type);
    }
    api::ObjectResult AllocateObject(const char*,const char*) override { throw std::runtime_error("AddComponent creates the object"); }
};
void Reset() {
    Check(handles.empty()); strings.clear(); arrays.clear();
    art.alive=head.alive=look.alive=true; private_look.clear(); private_nodes={&art,&head,&unrelated,&duplicate};
    head.name=source_head.name=L"头"; duplicate.name=L"unused";
    source_look.json=LR"({"head":{"m_FileID":2,"m_PathID":0},"curve":[1,2,3],"nullable":{"m_FileID":0,"m_PathID":0}})";
    look.json=L"{}";
    acquire=release=add=overwrite=destroy=0; fail_add=fail_write=wrong_reference=false;
}
}
void TestLookSupport() {
    static_assert(!std::is_copy_constructible_v<api::LookSupport>);
    const api::ReferenceMethods refs{M(Keep),M(Target),M(Free),M(Load),M(Release)};
    const api::UnityLifetimeMethods lifetime{M(Alive),M(Destroy)};
    api::MetadataResolver resolver({
        {"UnityEngine","JsonUtility",{
            {"ToJson","System.String",{"System.Object","System.Boolean"},true,M(Serialize)},
            {"FromJsonOverwrite","System.Void",{"System.String","System.Object"},true,M(Overwrite)}}},
        {"UnityEngine","GameObject",{{"Internal_AddComponentWithType","UnityEngine.Component",{"System.Type"},false,M(Add)}}},
        {"UnityEngine","Object",{{"GetInstanceID","System.Int32",{},false,M(InstanceId)}}},
        {"UnityEngine","Transform",{{"get_parent","UnityEngine.Transform",{},false,M(Parent)}}},
        {"UnityEngine","Component",{{"get_transform","UnityEngine.Transform",{},false,M(ComponentTransform)}}},
        {"System","Object",{{"GetType","System.Type",{},false,M(Type)}}},
        {"System","String",{
            {"get_Length","System.Int32",{},false,M(StringLength)},
            {"get_Chars","System.Char",{"System.Int32"},false,M(Character)}}}});
    const auto calls=api::ResolveComponentConfigMethods(resolver); Check(calls.binding_errors.empty());
    const api::ComponentConfig config(calls);
    const api::SceneObjects scene({M(Transform),M(Components),M(Name),{}, {}});
    const api::ManagedArrays managed_arrays({{},M(Length),M(Item),{}, {}});
    const api::VisualObjects visuals({{},{},{},M(GameObject),{}, {}});
    Objects objects;
    const api::LookSupportServices services{objects,scene,managed_arrays,visuals,config};
    std::wstring unicode=L"中文\U0001F600",empty,invalid(1,static_cast<wchar_t>(0xD800));
    Check(config.ReadText(&unicode).utf8=="中文\xF0\x9F\x98\x80" && config.ReadText(&empty).read.returned);
    Check(!config.ReadText(&invalid).read.returned);
    Reset();
    {
        api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(support.Status().prepared && support.CanSubmit() && add==1 && acquire==1 && overwrite==1);
        const auto result=nlohmann::json::parse(config.ReadText(&look.json).utf8);
        Check(result["head"]["m_FileID"]==head.id && result["curve"]==nlohmann::json({1,2,3}));
        Check(!support.Close().complete && destroy==0); // successful component belongs to Art
        art.alive=look.alive=false;
        Check(support.Close().complete && release==1 && handles.empty());
    }
    Reset();
    {
        private_look={&look}; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(support.Status().existing && add==0 && acquire==0 && support.Close().complete && destroy==0);
    }
    Reset();
    {
        head.name=L"different"; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(!support.Status().prepared && add==0 && support.CanSubmit() && support.Close().complete);
    }
    Reset();
    {
        duplicate.name=head.name; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(!support.Status().prepared && add==0 && support.Close().complete);
    }
    Reset();
    {
        source_look.json=LR"({"head":{"m_FileID":2,"m_PathID":99}})";
        api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(!support.Status().prepared && add==0 && support.Close().complete);
    }
    Reset();
    {
        head.name=source_head.name=std::wstring(300,L'长');
        api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(support.Status().prepared); look.alive=false; Check(support.Close().complete);
    }
    Reset();
    {
        fail_add=true; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(!support.Status().addition.call.returned && !support.Status().addition.object && !support.CanSubmit());
        Check(!support.SettleFailure() && destroy==1); // inspect graph after null out result
        Check(!support.SettleFailure() && destroy==1);
        look.alive=false; Check(support.SettleFailure() && support.CanSubmit() && support.Close().complete);
    }
    Reset();
    {
        wrong_reference=true; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(!support.Status().prepared && support.Status().rollback_needed && !support.SettleFailure());
        look.alive=false; Check(support.SettleFailure() && support.Close().complete);
    }
    Reset();
    {
        fail_write=true; api::LookSupport support(refs,lifetime); support.Prepare(services,&art,"npc.prefab");
        Check(support.Status().overwrite.exception==0xE0690002 && !support.SettleFailure());
        look.alive=false; Check(support.Close().complete && destroy==1);
    }
    Reset();
    std::cout<<"LookAt input support passed: authored settings, private reference mapping, existing support, partial Add rollback and delegated lifecycle; local substitutes only.\n";
}
