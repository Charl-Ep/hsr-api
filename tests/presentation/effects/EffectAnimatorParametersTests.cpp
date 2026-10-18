#include <hksr/presentation/effects/EffectAnimatorParameters.hpp>
#include <Windows.h>
#include <functional>
#include <map>
#include <limits>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr::api;
struct Request {unsigned id=17;bool released=false;void* effect=nullptr;} request,other{18};
struct Effect {void* request;};Effect effect{&request},replacement{&other};
struct Parameter {std::string name;int type;};
struct Animator {std::vector<void*> parameters;bool alive=true;};
Parameter wrong_name{"other",1},wrong_type{"value",3},floating{"value",1},integer{"value",3},boolean{"value",4},trigger{"value",9};
Animator first{{&wrong_name,&wrong_type}},second{{&floating,&integer,&boolean,&trigger}},third{{&floating}};
int info,game_object,animator_type,root_id=0,queries=0,reads=0,sets=0,frees=0;
std::string text;std::vector<void*> children;void* first_child=&first;void* selected=nullptr;
EffectParameterValue written;std::map<int,void*> roots;
std::function<void()> on_query,on_parameter,on_set;
bool throw_set=false,free_fault=false;
void CheckAt(bool v,int line){if(!v)throw std::runtime_error("effect parameter test failed at "+std::to_string(line));}
#define Check(v) CheckAt((v),__LINE__)
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
bool Kept(void* p){for(const auto& [_,v]:roots)if(p==v)return true;return false;}
int __fastcall Keep(void* p,int,int,void*){roots[++root_id]=p;return root_id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){++frees;if(free_fault)RaiseException(0xE0800002,0,0,nullptr);roots.erase(id);}
unsigned __fastcall Id(Request* p,void*){return p->id;}
bool __fastcall Released(Request* p,void*){return p->released;}
void* __fastcall Instance(Request* p,void*){return p->effect;}
bool __fastcall Alive(void* p,void*){Check(Kept(p));return p!=&first || first.alive;}
void* __fastcall Object(void* p,void* meta){Check(p==&effect && Kept(p) && meta==&info);return &game_object;}
void Query(void* p,void* type,bool inactive,void* meta){
    Check(p==&game_object && Kept(p) && type==&animator_type && Kept(type) && !inactive && meta==&info);
    ++queries;if(on_query)on_query();
}
void* __fastcall First(void* p,void* type,bool inactive,void* meta){Query(p,type,inactive,meta);return first_child;}
void* __fastcall Children(void* p,void* type,bool inactive,void* meta){Query(p,type,inactive,meta);return &children;}
int __fastcall Length(std::vector<void*>* p,void*){Check(Kept(p));return static_cast<int>(p->size());}
void* __fastcall Item(std::vector<void*>* p,int i,void*){Check(Kept(p));return p->at(i);}
void* __fastcall Parameters(Animator* p,void*){Check(Kept(p));++reads;if(on_parameter)on_parameter();return &p->parameters;}
void* __fastcall Name(Parameter* p,void*){Check(Kept(p));return &p->name;}
int __fastcall Type(Parameter* p,void*){Check(Kept(p));return p->type;}
bool __fastcall Equal(std::string* a,std::string* b,void*){Check(Kept(a) && Kept(b));return *a==*b;}
void Set(void* p,void* name,EffectParameterValue value,void* meta) {
    Check(Kept(p) && name==&text && Kept(name) && meta==&info);selected=p;written=value;++sets;
    if(on_set)on_set();if(throw_set)RaiseException(0xE0800001,0,0,nullptr);
}
void __fastcall Float(void* p,void* n,float value,void* m){Set(p,n,value,m);}
void __fastcall Integer(void* p,void* n,int value,void* m){Set(p,n,value,m);}
void __fastcall Bool(void* p,void* n,bool value,void* m){Set(p,n,value,m);}
void __fastcall Trigger(void* p,void* n,void* m){Set(p,n,EffectParameterTrigger{},m);}
struct Objects:ManagedObjects {
    ObjectResult NewStringUtf8(const char* value)override{text=value;return {{true,0},&text};}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("no task allocation");}
    ObjectResult ManagedType(const char* ns,const char* name)override{
        Check(std::string(ns)=="UnityEngine" && std::string(name)=="Animator");return {{true,0},&animator_type};
    }
};
}
void TestEffectAnimatorParameters(){
    MetadataResolver metadata({
        {"UnityEngine","Component",{{"get_gameObject","UnityEngine.GameObject",{},false,M(Object)}}},
        {"UnityEngine","GameObject",{
            {"GetComponentInChildren","UnityEngine.Component",{"System.Type","System.Boolean"},false,M(First)},
            {"GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"},false,M(Children)}}},
        {"UnityEngine","Object",{{"op_Implicit","System.Boolean",{"UnityEngine.Object"},true,M(Alive)}}},
        {"UnityEngine","Animator",{
            {"get_parameters","UnityEngine.AnimatorControllerParameter[]",{},false,M(Parameters)},
            {"SetFloat","System.Void",{"System.String","System.Single"},false,M(Float)},
            {"SetInteger","System.Void",{"System.String","System.Int32"},false,M(Integer)},
            {"SetBool","System.Void",{"System.String","System.Boolean"},false,M(Bool)},
            {"SetTrigger","System.Void",{"System.String"},false,M(Trigger)}}},
        {"UnityEngine","AnimatorControllerParameter",{
            {"get_name","System.String",{},false,M(Name)},
            {"get_type","UnityEngine.AnimatorControllerParameterType",{},false,M(Type)}}},
        {"System","String",{{"op_Equality","System.Boolean",{"System.String","System.String"},true,M(Equal)}}}});
    const auto methods=ResolveEffectAnimatorParameters460(metadata);Check(methods.binding_errors.empty());
    EffectAccessMethods access;access.id=M(Id);access.released=M(Released);access.instance=M(Instance);
    ReferenceMethods refs{M(Keep),M(Target),M(Free),{},{}};ManagedArrays arrays({{},M(Length),M(Item),{},{}});Objects objects;
    const std::vector<DeclaredField> fields{{"RPG.Client","MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request),6,true}};
    EffectAnimatorParameters parameters(EffectAccess(access),methods,refs,objects,arrays,fields);
    const EffectReference ref{17,0,&request};request.effect=&effect;children={&first,&second,&third};
    // FirstActive must not inspect names/types or continue to another Animator.
    Check(parameters.Set(ref,{"value",0.75f}).state==EffectControlState::Applied);
    Check(selected==&first && std::get<float>(written)==0.75f && reads==0 && sets==1 && roots.size()==1);
    const auto matching=EffectAnimatorSelection::FirstMatchingParameter;
    Check(parameters.Set(ref,{"value",0.25f,matching}).state==EffectControlState::Applied);
    Check(selected==&second && sets==2 && reads==2); // no third Animator broadcast
    Check(parameters.Set(ref,{"value",-7,matching}).state==EffectControlState::Applied && selected==&first && std::get<int>(written)==-7);
    Check(parameters.Set(ref,{"value",true,matching}).state==EffectControlState::Applied && selected==&second && std::get<bool>(written));
    Check(parameters.Set(ref,{"value",EffectParameterTrigger{},matching}).state==EffectControlState::Applied && selected==&second && std::holds_alternative<EffectParameterTrigger>(written));
    int before=sets;
    Check(parameters.Set(ref,{"absent",1.0f,matching}).state==EffectControlState::NoMatch && sets==before);
    first_child=nullptr;Check(parameters.Set(ref,{"value",false}).state==EffectControlState::NoMatch);first_child=&first;
    children.clear();Check(parameters.Set(ref,{"value",true,matching}).state==EffectControlState::NoMatch);children={&first,&second,&third};
    request.effect=nullptr;const auto previous_queries=queries;
    Check(parameters.Set(ref,{"value",1.0f}).state==EffectControlState::NotLoaded && queries==previous_queries);request.effect=&effect;
    on_query=[]{++request.id;};Check(parameters.Set(ref,{"value",1.0f}).state==EffectControlState::Ended && sets==before);on_query={};request.id=17;
    on_parameter=[]{request.effect=&replacement;};Check(parameters.Set(ref,{"value",1.0f,matching}).state==EffectControlState::Failed && sets==before);on_parameter={};request.effect=&effect;
    on_query=[]{effect.request=&other;};Check(parameters.Set(ref,{"value",1.0f}).state==EffectControlState::Failed && sets==before);on_query={};effect.request=&request;
    first.alive=false;Check(parameters.Set(ref,{"value",true}).state==EffectControlState::Failed && sets==before);first.alive=true;
    Check(parameters.Set(ref,{"value",std::numeric_limits<float>::infinity()}).state==EffectControlState::Failed && sets==before);
    Check(parameters.Set(ref,{std::string("x\0y",3),false}).state==EffectControlState::Failed && sets==before);
    // Reentry neither changes the typed input nor releases active-call roots.
    EffectAnimatorParameter input{"value",5.5f};
    on_query=[&]{input.value=99;Check(parameters.Set(ref,{"value",false}).state==EffectControlState::Failed);Check(!parameters.Close());};
    Check(parameters.Set(ref,input).state==EffectControlState::Applied && std::get<float>(written)==5.5f);on_query={};
    throw_set=true;const auto failed=parameters.Set(ref,{"value",true});throw_set=false;
    Check(failed.state==EffectControlState::Failed && failed.call.exception==0xE0800001 && roots.size()==1);
    Check(parameters.Close() && roots.empty());Check(parameters.Set(ref,{"value",false}).state==EffectControlState::Failed);
    // Missing matching-query methods must not block the independent first-child path.
    auto limited=methods;limited.parameters={};
    EffectAnimatorParameters minimal(EffectAccess(access),limited,refs,objects,arrays,fields);
    Check(minimal.Set(ref,{"value",true}).state==EffectControlState::Applied);
    Check(minimal.Set(ref,{"value",true,matching}).state==EffectControlState::Unsupported);
    Check(minimal.Close() && roots.empty());
    {
        EffectAnimatorParameters uncertain(EffectAccess(access),methods,refs,objects,arrays,fields);
        free_fault=true;const auto retained=uncertain.Set(ref,{"value",false});free_fault=false;
        Check(retained.state==EffectControlState::Applied && !retained.references_released && roots.size()>1);
        Check(!uncertain.Close());const auto after_close=frees;
        Check(!uncertain.Close() && frees==after_close); // no unknown decrement retry
    }
    roots.clear(); // end the isolated managed-domain fixture
    std::cout<<"Effect parameters: typed setters, first active/matching selection, exact receipt, deferred load, reentry and cleanup passed; no game playback.\n";
}
