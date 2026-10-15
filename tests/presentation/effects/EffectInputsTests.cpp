#include <hksr/presentation/effects/EffectInputs.hpp>
#include <Windows.h>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

namespace {
using namespace hksr::api;
int info,global_type,params_type,manager,other_manager,entity,callback;
int global_field,target_field,callback_field,serial=0,lookups=0;
void* service=&manager;
struct Parameters {void* target=nullptr;void* completion=nullptr;} parameters;
std::map<int,void*> roots;std::vector<std::unique_ptr<std::string>> texts;
bool fail_callback=false;
void Check(bool value){if(!value)throw std::runtime_error("effect inputs test failed");}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),&info};}
int __fastcall Keep(void* object,int,int,void*){roots[++serial]=object;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
void* __fastcall Find(void* type,void* name,int flags,void* meta) {
    Check(meta==&info);++lookups;const auto& key=*static_cast<std::string*>(name);
    if(type==&global_type){Check(flags==56 && key=="s_MonoEffectManager");return &global_field;}
    Check(type==&params_type && flags==52);
    if(key=="TargetEntity")return &target_field;
    Check(key=="CompleteCallback");return &callback_field;
}
void* __fastcall Get(void* field,void* object,void* meta){Check(field==&global_field && !object && meta==&info);return service;}
void __fastcall Set(void* field,void* object,void* value,int flags,void* binder,void* culture,void* meta) {
    Check(object==&parameters && !flags && !binder && !culture && meta==&info);
    if(field==&target_field){Check(value==&entity);parameters.target=value;}
    else {Check(field==&callback_field && value==&callback);parameters.completion=value;
        if(fail_callback)RaiseException(0xE0670001,0,0,nullptr);}
}
struct Inputs:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind)override {
        MetadataResolver resolver({{"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"},false,M(Find)}}}});
        return resolver.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void*)override {
        return {"Fixture","ActualFieldInfo",{
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void*)override{throw std::runtime_error("unused fields");}
    ObjectResult NewStringUtf8(const char* value)override{texts.push_back(std::make_unique<std::string>(value));return {{true,0},texts.back().get()};}
    ObjectResult ManagedType(const char* ns,const char* name)override {
        Check(std::string(ns)=="RPG.Client");
        if(std::string(name)=="GlobalVars")return {{true,0},&global_type};
        Check(std::string(name)=="TriggerEffectParams");return {{true,0},&params_type};
    }
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("unused allocation");}
};
}
void TestEffectInputs() {
    Inputs provider;EffectInputs inputs(provider,provider,provider,{M(Keep),M(Target),M(Free),{}, {}});
    Check(inputs.Manager().object==&manager);service=&other_manager;
    Check(inputs.Manager().object==&other_manager && lookups==1); // no cached service instance
    const auto first=inputs.Configure(&parameters,&entity,&callback);
    Check(first.target.returned && first.completion.returned && parameters.target==&entity && parameters.completion==&callback);
    Check(lookups==3);fail_callback=true;
    const auto partial=inputs.Configure(&parameters,&entity,&callback);fail_callback=false;
    Check(partial.target.returned && !partial.completion.returned && partial.completion.exception==0xE0670001 && lookups==3);
    Check(!inputs.Configure(&parameters,nullptr,&callback).target.returned);
    Check(inputs.Close() && roots.empty() && !inputs.Manager().call.returned);texts.clear();
}
