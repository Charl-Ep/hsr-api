#include <hksr/presentation/events/EventValueInputs.hpp>
#include <Windows.h>
#include <map>
#include <memory>
#include <stdexcept>
using namespace hksr::api;
namespace {
struct Obj {std::string type,text;std::map<std::string,Obj*> fields;std::map<int,int> values;std::map<int,std::string> strings;};
std::vector<std::unique_ptr<Obj>> arena;std::map<int,void*> roots;int serial=0,info;
Obj *global=nullptr,*config=nullptr,*fault_owner=nullptr;bool fail_strings=false;
void Check(bool v){if(!v)throw std::runtime_error("event values test failed");}
Obj* New(const char* type){auto p=std::make_unique<Obj>();p->type=type;auto* out=p.get();arena.push_back(std::move(p));return out;}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* p,int,int,void*){roots[++serial]=p;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
Obj* __fastcall Find(Obj*,Obj* name,int flags,void*){Check(flags==52 || flags==56);auto* f=New("Field");f->text=name->text;return f;}
Obj* __fastcall Get(Obj* f,Obj* o,void*){if(!o){Check(f->text=="GameCoreConstValueConfig");return global;}return o->fields[f->text];}
void __fastcall Set(Obj* f,Obj* o,Obj* v,int,void*,void*,void*) {
    o->fields[f->text]=v;
    if(fail_strings && o==fault_owner && f->text=="KINCCPGLBEC") {fail_strings=false;RaiseException(0xE0760101,0,0,nullptr);}
}
Obj* __fastcall Context(Obj* o,void*){return o->fields["context"];}
Obj* __fastcall Custom(Obj* o,void*){return o->fields["custom"];}
Obj* __fastcall Load(Obj* path,void*){Check(path->text=="Config/Avatar_Test.json");return config;}
void __fastcall Init(Obj* o,void*){o->fields["KFKGOMIMNLD"]=New("Values");o->fields["KINCCPGLBEC"]=New("Dictionary");}
void __fastcall Merge(Obj* o,Obj* from,void*){if(from)for(auto p:from->values)o->fields["KFKGOMIMNLD"]->values[p.first]=p.second;}
void __fastcall MergeText(Obj* o,Obj* from,void*){if(from)for(auto p:from->strings)o->fields["KINCCPGLBEC"]->strings[p.first]=p.second;}
void __fastcall CopyText(Obj* o,Obj* from,void*){o->strings=from->strings;}
struct Provider:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind)override {
        MetadataResolver r({
            {"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"},false,M(Find)}}},
            {"","LGBHONLHNPN",{{"get_TaskContext","RPG.GameCore.TaskContext",{},false,M(Context)}}},
            {"RPG.GameCore","TaskContext",{{"GetOwnerCustomData","JBFFCBPMKEF",{},false,M(Custom)}}},
            {"RPG.GameCore","GameCoreConfigLoader",{{"LoadCharacterConfig","RPG.GameCore.CharacterConfig",{"System.String"},true,M(Load)}}},
            {"","FODPNNIJCJI",{{".ctor","System.Void",{},false,M(Init)},
                {"GDEJIHHKNBI","System.Void",{"ENEIHJJPNPI"},false,M(Merge)},
                {"CMEAAFAOMNJ","System.Void",{"System.Collections.Generic.Dictionary<System.String,System.String>"},false,M(MergeText)}}}});
        return r.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void* p)override {
        if(static_cast<Obj*>(p)->type=="Dictionary")return {"System.Collections.Generic","Dictionary`2",{{".ctor","System.Void",{"System.Collections.Generic.IDictionary<RPG.GameCore.StringHash,System.String>"},false,M(CopyText)}}};
        Check(static_cast<Obj*>(p)->type=="Field");return {"Fixture","Field",{
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void*)override{throw std::runtime_error("unused field scan");}
    ObjectResult NewStringUtf8(const char* s)override{auto* p=New("String");p->text=s;return {{true,0},p};}
    ObjectResult ManagedType(const char*,const char* t)override{auto* p=New("Type");p->text=t;return {{true,0},p};}
    ObjectResult AllocateObject(const char* ns,const char* type)override{Check(std::string(ns).empty() && std::string(type)=="FODPNNIJCJI");return {{true,0},New("Custom")};}
};
Obj* Component(Obj*& owner) {auto* c=New("Component");auto* ctx=New("Context");owner=New("Custom");Init(owner,nullptr);c->fields["context"]=ctx;ctx->fields["custom"]=owner;return c;}
}
void TestEventValueInputs() {
    Provider p;ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    global=New("Global");config=New("Config");auto* g=New("Values");g->values={{2,7},{3,10}};global->fields["AllCharacterDynamicValues"]=g;
    auto* v=New("Values");v->values={{3,40},{4,0}};config->fields["DynamicValues"]=v;
    auto* t=New("Strings");t->strings={{100,"skin"}};config->fields["DynamicStrings"]=t;
    Obj *a,*b;auto* ca=Component(a);auto* cb=Component(b);
    auto* baseline=a->fields["KFKGOMIMNLD"];baseline->values={{1,5}};
    auto* text=a->fields["KINCCPGLBEC"];text->strings={{99,"keep"}};
    EventValueInputs first(p,p,p,refs),second(p,p,p,refs);
    Check(first.Apply(ca,"Config/Avatar_Test.json") && second.Apply(cb,"Config/Avatar_Test.json"));
    auto* one=a->fields["KFKGOMIMNLD"];auto* two=b->fields["KFKGOMIMNLD"];
    Check(one!=two && one!=v && one!=baseline && one->values==std::map<int,int>{{1,5},{2,7},{3,40},{4,0}});
    Check(a->fields["KINCCPGLBEC"]!=text && a->fields["KINCCPGLBEC"]->strings.size()==2);
    one->values[4]=99;Check(two->values.at(4)==0 && v->values.at(4)==0 && baseline->values.size()==1);
    Check(first.Restore() && a->fields["KFKGOMIMNLD"]==baseline && a->fields["KINCCPGLBEC"]==text);
    Check(first.Close(false) && second.Close(false) && roots.empty());
    EventValueInputs partial(p,p,p,refs);fault_owner=a;fail_strings=true;
    Check(!partial.Apply(ca,"Config/Avatar_Test.json"));
    Check(a->fields["KFKGOMIMNLD"]==baseline && a->fields["KINCCPGLBEC"]==text && partial.Close(false) && roots.empty());
    EventValueInputs changed(p,p,p,refs);Check(changed.Apply(ca,"Config/Avatar_Test.json"));
    auto* foreign=New("Values");a->fields["KFKGOMIMNLD"]=foreign;
    Check(!changed.Restore() && a->fields["KFKGOMIMNLD"]==foreign && !changed.Close(false));
    Check(changed.Close(true) && roots.empty()); // no write to the retired owner
    arena.clear();
}
