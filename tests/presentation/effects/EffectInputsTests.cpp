#include <hksr/presentation/effects/EffectInputs.hpp>
#include <Windows.h>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>
#include <cstddef>
#include <limits>

namespace {
using namespace hksr::api;
int info,global_type,params_type,manager,other_manager,entity,callback;
int global_field,target_field,callback_field,caster_field,name_field,attach_field,other_target,serial=0,lookups=0;
int flags_field,flag_type,flag_box,flag_array,flag_writes=0;
void* service=&manager;
struct NullableVector {Position3 value{};std::uint8_t present=0,padding[3]{};};
struct NullableRotation {Rotation4 value{};std::uint8_t present=0,padding[3]{};};
struct Parameters {
    void* target=nullptr;void* completion=nullptr;void* caster=nullptr;void* name=nullptr;void* attach=nullptr;
    NullableVector offset,rotation,scale;std::uint8_t fade=0,simulate=0;std::int32_t usage=0;bool attached=false;float pitch=0;
    NullableVector initial,anchor;NullableRotation orientation;std::uint8_t ignore_prefab=0;
    void* flags=nullptr;
} parameters;
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
    if(key=="CasterEntity")return &caster_field;
    if(key=="UniqueEffectName")return &name_field;
    if(key=="DynamicAttach")return &attach_field;
    if(key=="Flags")return &flags_field;
    Check(key=="CompleteCallback");return &callback_field;
}
void* __fastcall Get(void* field,void* object,void* meta){Check(field==&global_field && !object && meta==&info);return service;}
void __fastcall Set(void* field,void* object,void* value,int flags,void* binder,void* culture,void* meta) {
    Check(object==&parameters && !flags && !binder && !culture && meta==&info);
    if(field==&target_field){Check(!value || value==&entity || value==&other_target);parameters.target=value;}
    else if(field==&caster_field){Check(value==&entity);parameters.caster=value;}
    else if(field==&name_field){parameters.name=value;}
    else if(field==&attach_field){parameters.attach=value;}
    else if(field==&flags_field){Check(!value || value==&flag_array);parameters.flags=value;}
    else {Check(field==&callback_field && value==&callback);parameters.completion=value;
        if(fail_callback)RaiseException(0xE0670001,0,0,nullptr);}
}
void __fastcall Attach(void* object,bool enabled,void*) {Check(object==&parameters);parameters.attached=enabled && parameters.target;}
void* __fastcall BoxFlag(void* type,int value,void* metadata){Check(type==&flag_type && value==1 && metadata==&info);return &flag_box;}
void* __fastcall CreateFlags(void* type,int count,void* metadata){Check(type==&flag_type && count==1 && metadata==&info);return &flag_array;}
int __fastcall ArrayLength(void* array,void*){Check(array==&flag_array);return 1;}
void* __fastcall ArrayGet(void* array,int index,void*){Check(array==&flag_array && index==0);return &flag_box;}
void __fastcall ArraySet(void* array,void* value,int index,void* metadata){Check(array==&flag_array && value==&flag_box && index==0 && metadata==&info);++flag_writes;}
struct Inputs:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind)override {
        MetadataResolver resolver({{"System","RuntimeType",{{"GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"},false,M(Find)}}},
            {"RPG.Client","TriggerEffectParams",{{"set_IsAttachToTargetEntity","System.Void",{"System.Boolean"},false,M(Attach)}}},
            {"System","Enum",{{"ToObject","System.Object",{"System.Type","System.Int32"},true,M(BoxFlag)}}},
            {"System","Array",{{"CreateInstance","System.Array",{"System.Type","System.Int32"},true,M(CreateFlags)},
                {"get_Length","System.Int32",{},false,M(ArrayLength)},{"GetValue","System.Object",{"System.Int32"},false,M(ArrayGet)},
                {"SetValue","System.Void",{"System.Object","System.Int32"},false,M(ArraySet)}}}});
        return resolver.Resolve(ns,type,name,result,args,kind);
    }
    DeclaredType ReadObjectType(void*)override {
        return {"Fixture","ActualFieldInfo",{
            {"GetValue","System.Object",{"System.Object"},false,M(Get)},
            {"SetValue","System.Void",{"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"},false,M(Set)}}};
    }
    RuntimeTypeName ReadObjectTypeName(void* object)override {Check(object==&parameters);return {"RPG.Client","TriggerEffectParams"};}
    std::vector<DeclaredField> ReadObjectFields(void* object)override {
        Check(object==&parameters);
        auto field=[](const char* name,const char* type,std::size_t offset){return DeclaredField{"RPG.Client","TriggerEffectParams",name,type,offset,6,true};};
        return {field("PositionOffset","System.Nullable<UnityEngine.Vector3>",offsetof(Parameters,offset)),
            field("RotationOffset","System.Nullable<UnityEngine.Vector3>",offsetof(Parameters,rotation)),
            field("CustomScale","System.Nullable<UnityEngine.Vector3>",offsetof(Parameters,scale)),
            field("IsNeedFadeIn","System.Boolean",offsetof(Parameters,fade)),
            field("ForceSimulateImmediately","System.Boolean",offsetof(Parameters,simulate)),
            field("ParamEntityUsage","RPG.GameCore.EffectParamEntityUsage",offsetof(Parameters,usage)),
            field("TowardMaxPitchAngle","System.Single",offsetof(Parameters,pitch)),
            field("InitPosition","System.Nullable<UnityEngine.Vector3>",offsetof(Parameters,initial)),
            field("AnchorPosition","System.Nullable<UnityEngine.Vector3>",offsetof(Parameters,anchor)),
            field("InitRotation","System.Nullable<UnityEngine.Quaternion>",offsetof(Parameters,orientation)),
            field("IgnorePrefabAttachPoint","System.Boolean",offsetof(Parameters,ignore_prefab))};
    }
    ObjectResult NewStringUtf8(const char* value)override{texts.push_back(std::make_unique<std::string>(value));return {{true,0},texts.back().get()};}
    ObjectResult ManagedType(const char* ns,const char* name)override {
        if(std::string(ns)=="RPG.GameCore" && std::string(name)=="MonoEffectFlag")return {{true,0},&flag_type};
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
    EntityEffectOptions options;options.unique_name="scene";options.position_offset={1,2,3};options.rotation_offset={0,90,0};
    options.fade_in=options.simulate_immediately=options.attach_to_target=true;options.scale=Position3{2,3,4};
    options.attach_point="Muzzle";options.resident=true;
    const auto full=inputs.ConfigureEntity(&parameters,&entity,&other_target,&callback,options);
    Check(full.target.returned && full.completion.returned && parameters.caster==&entity && parameters.target==&other_target);
    Check(*static_cast<std::string*>(parameters.name)=="scene" && parameters.offset.present && parameters.offset.value.z==3);
    Check(parameters.rotation.present && parameters.rotation.value.y==90 && parameters.scale.present && parameters.scale.value.x==2);
    Check(parameters.fade==1 && parameters.simulate==1 && parameters.usage==1 && parameters.attached && parameters.pitch==90);
    Check(parameters.attach && *static_cast<std::string*>(parameters.attach)=="Muzzle");
    Check(parameters.flags==&flag_array && flag_writes==1);
    const auto persistent_roots=roots.size();options.scale.reset();options.attach_to_target=false;
    Check(inputs.ConfigureEntity(&parameters,&entity,&other_target,&callback,options).completion.returned);
    Check(!parameters.scale.present && !parameters.attached && roots.size()==persistent_roots);
    Check(parameters.attach && *static_cast<std::string*>(parameters.attach)=="Muzzle"); // initial point without parenting
    options.attach_point.clear();options.resident=false;Check(inputs.ConfigureEntity(&parameters,&entity,&other_target,&callback,options).completion.returned);
    Check(!parameters.attach && roots.size()==persistent_roots);
    Check(!parameters.flags && flag_writes==1);
    options.attach_point="Muzzle";
    fail_callback=true;const auto failed=inputs.ConfigureEntity(&parameters,&entity,&other_target,&callback,options);fail_callback=false;
    Check(failed.target.returned && !failed.completion.returned && roots.size()==persistent_roots);
    Check(!inputs.ConfigureEntity(&parameters,&entity,nullptr,&callback,options).target.returned);
    options.position_offset.x=std::numeric_limits<float>::quiet_NaN();bool rejected=false;
    try{inputs.ConfigureEntity(&parameters,&entity,&other_target,&callback,options);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected && roots.size()==persistent_roots);
    // A spatial request keeps the caster, but never makes it the target.
    SpatialEffectOptions spatial;spatial.position={7,-2,59};spatial.rotation={0,2,0,0};
    spatial.unique_name="world";spatial.fade_in=false;spatial.simulate_immediately=true;spatial.resident=true;
    const auto placed=inputs.ConfigureSpatial(&parameters,&entity,&callback,spatial);
    Check(placed.target.returned && placed.completion.returned && !parameters.target && parameters.caster==&entity);
    Check(parameters.initial.present && parameters.initial.value.z==59 && parameters.anchor.present && parameters.anchor.value.y==-2);
    Check(parameters.orientation.present && parameters.orientation.value.y==1 && parameters.orientation.value.w==0);
    Check(parameters.ignore_prefab && !parameters.attached && !parameters.attach && !parameters.offset.present && !parameters.rotation.present);
    Check(!parameters.fade && parameters.simulate && !parameters.scale.present);
    Check(parameters.flags==&flag_array && flag_writes==1);
    const auto spatial_roots=roots.size();fail_callback=true;
    Check(!inputs.ConfigureSpatial(&parameters,&entity,&callback,spatial).completion.returned);fail_callback=false;
    Check(roots.size()==spatial_roots);
    spatial.rotation={0,0,0,0};rejected=false;
    try{inputs.ConfigureSpatial(&parameters,&entity,&callback,spatial);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected && roots.size()==spatial_roots);
    Check(inputs.Close() && roots.empty() && !inputs.Manager().call.returned);texts.clear();
}
