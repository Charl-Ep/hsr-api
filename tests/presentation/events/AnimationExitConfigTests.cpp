#include <hksr/presentation/events/AnimationExitConfig.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <Windows.h>

namespace {
using namespace hksr::api;
struct N {
    std::string type,text,owner;
    std::map<std::string,N*> fields;
    std::vector<void*> items;
};
std::vector<std::unique_ptr<N>> nodes;
std::map<int,void*> roots;std::map<std::string,N*> abilities;
int info,serial=0,clones=0,constructors=0;bool clone_fault=false;
void Check(bool v){if(!v)throw std::runtime_error("animation exit config test failed");}
N* New(std::string type){auto p=std::make_unique<N>();p->type=std::move(type);auto* out=p.get();nodes.push_back(std::move(p));return out;}
N* Text(std::string value){auto* n=New("System.String");n->text=std::move(value);return n;}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* p,int old,int kind,void* m){Check(!old && kind==2 && m==&info);roots[++serial]=p;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1);}
void* __fastcall Load(N* key,void*){auto it=abilities.find(key->text);return it==abilities.end()?nullptr:it->second;}
void* __fastcall Clone(N* n,void*){if(clone_fault)RaiseException(0xE0670021,0,0,nullptr);auto* c=New(n->type);*c=*n;++clones;return c;}
void __fastcall Construct(N*,void*){++constructors;}
void* __fastcall Field(N* type,N* key,int flags,void*){Check(flags==54);auto* f=New("System.Reflection.FixtureField");f->owner=type->text;f->text=key->text;return f;}
void* __fastcall Get(N* field,N* n,void*){auto i=n->fields.find(field->text);if(i==n->fields.end())throw std::runtime_error("missing fixture field");return i->second;}
void __fastcall Set(N* field,N* n,N* value,int flags,void* binder,void* culture,void*){Check(flags==0 && !binder && !culture);n->fields[field->text]=value;}
bool __fastcall Equal(N* a,N* b,void*){return a && b && a->text==b->text;}
void* __fastcall Convert(N* text,N* type,void*){auto* v=New(type->text);v->text=text->text;return v;}
void* __fastcall Create(N* type,int count,void*){auto* a=New(type->text+"[]");a->items.resize(count);return a;}
int __fastcall Length(N* a,void*){return static_cast<int>(a->items.size());}
void* __fastcall Item(N* a,int i,void*){return a->items.at(i);}
void __fastcall SetItem(N* a,void* value,int i,void*){a->items.at(i)=value;}
struct Inputs:MethodResolver,ObjectMetadata,ManagedObjects,TypeRelationships {
    bool TypeDerivesFrom(const char*,const char* type,const char* space,const char* base)override{
        Check(std::string(space)=="System" && std::string(base)=="ValueType" &&
            (std::string(type)=="AnimGroupEventConfig" || std::string(type)=="AnimGroupEventEntry"));return false;
    }
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> args,MethodKind kind) override {
        const std::string t=type,n=name,r=result;
        if(n=="GetTurnBasedAbility"){Check(t=="AbilityJsonTable" && kind==MethodKind::Static);return M(Load);}
        if(n=="MemberwiseClone"){Check(t=="Object" && args.size()==0);return M(Clone);}
        if(n==".ctor"){Check(std::string(ns)=="RPG.GameCore" && args.size()==0);return M(Construct);}
        if(n=="GetField"){Check(t=="RuntimeType" && args.size()==2);return M(Field);}
        if(n=="Equals"){Check(t=="String" && kind==MethodKind::Static && r=="System.Boolean");return M(Equal);}
        if(n=="ChangeType"){Check(t=="Convert" && args.size()==2 && r=="System.Object");return M(Convert);}
        throw std::runtime_error("unexpected exit config method");
    }
    RuntimeTypeName ReadObjectTypeName(void* object) override {
        const auto& name=static_cast<N*>(object)->type;auto dot=name.rfind('.');
        return {name.substr(0,dot),name.substr(dot+1)};
    }
    DeclaredType ReadObjectType(void* object) override {
        const auto name=ReadObjectTypeName(object);DeclaredType t{name.name_space,name.name,{}};
        if(t.name=="FixtureField")t.methods={
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}};
        return t;
    }
    std::vector<DeclaredField> ReadObjectFields(void*)override{return {};}
    ObjectResult NewStringUtf8(const char* s)override{return {{true,0},Text(s)};}
    ObjectResult ManagedType(const char* ns,const char* n)override{auto* type=New("System.RuntimeType");type->text=std::string(ns)+"."+n;return {{true,0},type};}
    ObjectResult AllocateObject(const char* ns,const char* n)override{return {{true,0},New(std::string(ns)+"."+n)};}
};
}
void TestAnimationExitConfig() {
    Inputs input;const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    const ArrayMethods arrays{M(Create),M(Length),M(Item),M(SetItem),{}};
    auto* root=New("RPG.GameCore.TurnBasedAbilityConfig");abilities["ability"]=root;
    auto* branch=New("RPG.GameCore.Branch"),*list=New("RPG.GameCore.TaskConfig[]");
    auto* source=New("RPG.GameCore.RemoveEffect"),*alias=New("RPG.GameCore.TargetAlias");
    alias->fields["Alias"]=Text("Caster");source->fields["TargetType"]=alias;
    source->fields["Flags"]=New("RPG.GameCore.MonoEffectFlag[]"); // must preserve original selectors
    source->fields["EffectPath"]=Text("Effects/AnyRole");source->fields["Unbind"]=Text("true");
    source->fields["FadeOutRegionStart"]=Text("0.37"); // builder must not flatten or reinterpret values
    list->items={source};branch->fields["Tasks"]=list;root->fields["Branch"]=branch;
    EffectRemovalSource removal;removal.ability="ability";removal.path={
        {"RPG.GameCore.TurnBasedAbilityConfig","Branch"},
        {"RPG.GameCore.Branch","Tasks",ConfigSelection::Index,0,{}}};
    {
        AnimationExitConfig config(input,input,input,input,refs,arrays);
        Check(config.Prepare("UnrelatedStateName",{removal}) && config.Errors().empty() && clones==1 && constructors==2);
        auto* groups=static_cast<N*>(config.Groups());Check(groups->items.size()==1);
        auto* group=static_cast<N*>(groups->items[0]);auto* states=group->fields.at("States");
        auto* state=static_cast<N*>(states->items[0]);
        Check(state->fields.at("Name")->text=="UnrelatedStateName" && state->fields.at("DisallowExitTriggeredOnStateUpdate")->text=="True");
        Check(group->fields.at("OnEnter")->items.empty() && group->fields.at("OnExit")->items.size()==1);
        auto* copied=static_cast<N*>(group->fields.at("OnExit")->items[0]);
        Check(copied!=source && copied->fields==source->fields && source->fields.size()==5);
        Check(!config.Prepare("again",{removal}) && config.Close() && config.Close() && roots.empty());
    }
    {
        auto bad=removal;bad.ability="not loaded";
        AnimationExitConfig config(input,input,input,input,refs,arrays);
        Check(config.Prepare("State",{bad,removal}) && config.Errors().size()==1); // independent valid sibling remains
        Check(config.Close() && roots.empty());
    }
    alias->fields["Alias"]=Text("AnotherEntity");
    {
        AnimationExitConfig config(input,input,input,input,refs,arrays);
        Check(!config.Prepare("State",{removal}) && !config.Groups() && config.Close() && roots.empty());
    }
    alias->fields["Alias"]=Text("Caster");clone_fault=true;
    {
        AnimationExitConfig config(input,input,input,input,refs,arrays);
        Check(!config.Prepare("State",{removal}) && config.Close() && roots.empty());
    }
    clone_fault=false;
}
