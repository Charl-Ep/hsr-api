#include <hksr/presentation/effects/EffectInputs.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct EffectInputs::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;ManagedRoots roots;
    std::vector<ManagedRootId> retained;
    struct Field {void* object=nullptr;Method call;};
    std::map<std::string,Field> fields;
    std::map<std::string,DeclaredField> entity_fields;
    Method attach;
    std::map<int,void*> flag_arrays;
    bool spatial_prepared=false;
    bool closed=false;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r):methods(m),metadata(d),objects(o),roots(r){}
    void* Keep(ObjectResult value) {
        if(!value.call.returned || !value.object)throw std::runtime_error("effect field object unavailable");
        const auto id=roots.Keep(value.object);retained.push_back(id);
        if(!roots.Status(id).ready)throw std::runtime_error("effect field retention failed");
        return value.object;
    }
    Field Get(const char* owner,const char* name,bool is_static) {
        const auto key=std::string(owner)+"."+name;
        if(const auto found=fields.find(key);found!=fields.end())return found->second;
        auto* type=Keep(objects.ManagedType("RPG.Client",owner));
        auto* text=Keep(objects.NewStringUtf8(name));
        const auto find=methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"});
        ObjectResult found;found.call=native::Call(find,&found.object,type,text,is_static?56:52);
        auto* field=Keep(found);const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
        Field result;result.object=field;
        if(is_static)result.call=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetValue","System.Object",{"System.Object"});
        else result.call=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
        fields.emplace(key,result);return result;
    }
    void PrepareEntity(void* parameters) {
        if(attach.code)return;
        const auto type=metadata.ReadObjectTypeName(parameters);
        if(type.name_space!="RPG.Client" || type.name!="TriggerEffectParams")throw std::invalid_argument("entity effect parameters type differs");
        const auto declared=metadata.ReadObjectFields(parameters);
        std::map<std::string,DeclaredField> resolved;
        for(const auto& item:std::vector<std::pair<const char*,const char*>>{
            {"PositionOffset","System.Nullable<UnityEngine.Vector3>"},{"RotationOffset","System.Nullable<UnityEngine.Vector3>"},
            {"CustomScale","System.Nullable<UnityEngine.Vector3>"},{"IsNeedFadeIn","System.Boolean"},
            {"ForceSimulateImmediately","System.Boolean"},{"IsEnableReplay","System.Boolean"},{"ParamEntityUsage","RPG.GameCore.EffectParamEntityUsage"},
            {"TowardMaxPitchAngle","System.Single"}}) {
            const DeclaredField* field=nullptr;
            for(const auto& value:declared)if(value.owner_namespace=="RPG.Client" && value.owner_name=="TriggerEffectParams" && value.name==item.first) {
                if(field || value.type!=item.second || !value.flags_observed || (value.flags&0x10))
                    throw std::runtime_error(std::string("entity effect field differs: ")+item.first);
                field=&value;
            }
            if(!field)throw std::runtime_error(std::string("entity effect field absent: ")+item.first);
            resolved.emplace(item.first,*field);
        }
        auto method=methods.Resolve("RPG.Client","TriggerEffectParams","set_IsAttachToTargetEntity","System.Void",{"System.Boolean"});
        entity_fields=std::move(resolved);attach=method;
    }
    void PrepareSpatial(void* parameters) {
        PrepareEntity(parameters);
        if(spatial_prepared)return;
        const auto declared=metadata.ReadObjectFields(parameters);
        auto resolved=entity_fields;
        for(const auto& item:std::vector<std::pair<const char*,const char*>>{
            {"InitPosition","System.Nullable<UnityEngine.Vector3>"},
            {"AnchorPosition","System.Nullable<UnityEngine.Vector3>"},
            {"InitRotation","System.Nullable<UnityEngine.Quaternion>"},
            {"IgnorePrefabAttachPoint","System.Boolean"}}) {
            const DeclaredField* field=nullptr;
            for(const auto& value:declared)if(value.owner_namespace=="RPG.Client" && value.owner_name=="TriggerEffectParams" && value.name==item.first) {
                if(field || value.type!=item.second || !value.flags_observed || (value.flags&0x10))
                    throw std::runtime_error(std::string("spatial effect field differs: ")+item.first);
                field=&value;
            }
            if(!field)throw std::runtime_error(std::string("spatial effect field absent: ")+item.first);
            resolved.emplace(item.first,*field);
        }
        entity_fields=std::move(resolved);spatial_prepared=true;
    }
    template<class T>void Scalar(void* parameters,const char* name,const T& value) {
        // Only audited blittable fields on a private, unsubmitted parameter
        // object. Managed reference writes always use FieldInfo.SetValue.
        if(!native::CopyMemoryChecked(static_cast<unsigned char*>(parameters)+entity_fields.at(name).offset,&value,sizeof(value)))
            throw std::runtime_error(std::string("entity effect field write failed: ")+name);
    }
    CallResult Set(Field field,void* parameters,void* value) {
        return native::CallVoid(field.call,field.object,parameters,value,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr));
    }
    void ReleaseValue(ManagedRootId id) {
        if(!id)return;
        if(roots.Release(id).complete){roots.Forget(id);retained.erase(std::remove(retained.begin(),retained.end(),id),retained.end());}
    }
    void* FlagArray(int flag) {
        if(const auto it=flag_arrays.find(flag);it!=flag_arrays.end())return it->second;
        auto* type=Keep(objects.ManagedType("RPG.GameCore","MonoEffectFlag"));
        const auto type_root=retained.back();
        ManagedRootId value_root=0,array_root=0;
        try {
            const auto box=methods.Resolve("System","Enum","ToObject","System.Object",{"System.Type","System.Int32"},MethodKind::Static);
            ObjectResult boxed;boxed.call=native::Call(box,&boxed.object,type,std::int32_t{flag});
            auto* value=Keep(boxed);value_root=retained.back();
            ManagedArrays arrays(ResolveArrayMethods(methods));
            auto* array=Keep(arrays.Create(type,1));array_root=retained.back();
            if(!arrays.Set(array,value,0).returned)throw std::runtime_error("effect resident array initialization failed");
            // Config tasks also share immutable Flags arrays with requests.
            flag_arrays.emplace(flag,array);
        }catch(...){ReleaseValue(array_root);ReleaseValue(value_root);ReleaseValue(type_root);throw;}
        ReleaseValue(value_root);ReleaseValue(type_root);return flag_arrays.at(flag);
    }
};
EffectInputs::EffectInputs(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r):state_(new State(m,d,o,r)){}
EffectInputs::~EffectInputs(){delete state_;}
ObjectResult EffectInputs::Manager() {
    if(state_->closed)return {};
    const auto field=state_->Get("GlobalVars","s_MonoEffectManager",true);
    ObjectResult out;out.call=native::Call(field.call,&out.object,field.object,static_cast<void*>(nullptr));return out;
}
CallResult EffectInputs::ConfigureReplay(void* parameters,bool enabled) {
    if(state_->closed || !parameters)return {};
    state_->PrepareEntity(parameters);
    state_->Scalar(parameters,"IsEnableReplay",static_cast<std::uint8_t>(enabled));
    return {true,0};
}
EffectInputResult EffectInputs::Configure(void* parameters,void* target,void* completion) {
    EffectInputResult out;if(state_->closed || !parameters || !target || !completion)return out;
    // Resolve BOTH before either assignment. Failure after a write still
    // belongs to the caller's unsubmitted request, not the official loader.
    const auto entity=state_->Get("TriggerEffectParams","TargetEntity",false);
    const auto callback=state_->Get("TriggerEffectParams","CompleteCallback",false);
    out.target=native::CallVoid(entity.call,entity.object,parameters,target,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr));
    if(out.target.returned)out.completion=native::CallVoid(callback.call,callback.object,parameters,completion,0,
        static_cast<void*>(nullptr),static_cast<void*>(nullptr));
    return out;
}
EffectInputResult EffectInputs::ConfigureEntity(void* parameters,void* caster,void* target,void* completion,const EntityEffectOptions& options) {
    if(!target)return {};
    const auto captured=options;
    return ConfigureImpl(parameters,caster,target,completion,captured,nullptr);
}
EffectInputResult EffectInputs::ConfigureHitReferences(void* parameters,void* caster,void* target,void* completion) {
    auto& s=*state_;EffectInputResult out;
    if(s.closed || !parameters || !caster || !target || !completion)return out;
    const auto caster_field=s.Get("TriggerEffectParams","CasterEntity",false);
    const auto target_field=s.Get("TriggerEffectParams","TargetEntity",false);
    const auto callback_field=s.Get("TriggerEffectParams","CompleteCallback",false);
    out.target=s.Set(caster_field,parameters,caster);
    if(out.target.returned)out.target=s.Set(target_field,parameters,target);
    if(out.target.returned)out.completion=s.Set(callback_field,parameters,completion);
    return out;
}
EffectInputResult EffectInputs::ConfigureSpatial(void* parameters,void* caster,void* completion,const SpatialEffectOptions& input) {
    auto spatial=input;
    const auto& r=spatial.rotation;
    const double squared=double(r.x)*r.x+double(r.y)*r.y+double(r.z)*r.z+double(r.w)*r.w;
    if(!std::isfinite(spatial.position.x) || !std::isfinite(spatial.position.y) || !std::isfinite(spatial.position.z) ||
        !std::isfinite(squared) || squared==0)throw std::invalid_argument("invalid spatial effect pose");
    const auto length=std::sqrt(squared);
    spatial.rotation={float(r.x/length),float(r.y/length),float(r.z/length),float(r.w/length)};
    EntityEffectOptions options;options.unique_name=spatial.unique_name;options.scale=spatial.scale;
    options.fade_in=spatial.fade_in;options.simulate_immediately=spatial.simulate_immediately;
    options.resident=spatial.resident;
    return ConfigureImpl(parameters,caster,nullptr,completion,options,&spatial);
}
EffectInputResult EffectInputs::ConfigureImpact(void* parameters,void* caster,void* target,void* completion,const ImpactEffectOptions& options) {
    auto& s=*state_;EffectInputResult out;
    if(s.closed || !parameters || !caster || !target || !completion)return out;
    const auto finite=[](Position3 p){return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);};
    if(!finite(options.position) || !finite(options.direction))throw std::invalid_argument("invalid impact pose");
    s.PrepareSpatial(parameters);
    Rotation4 rotation{};
    if(options.align) {
        auto forward=options.direction;Position3 up{0,1,0};
        const auto method=s.methods.Resolve("UnityEngine","Quaternion","LookRotation_Injected","System.Void",
            {"UnityEngine.Vector3&","UnityEngine.Vector3&","UnityEngine.Quaternion&"},MethodKind::Static);
        if(!native::CallVoid(method,&forward,&up,&rotation).returned)throw std::runtime_error("impact orientation unavailable");
    }
    const auto caster_field=s.Get("TriggerEffectParams","CasterEntity",false);
    const auto param_field=s.Get("TriggerEffectParams","ParamEntity",false);
    const auto target_field=s.Get("TriggerEffectParams","TargetEntity",false);
    const auto callback_field=s.Get("TriggerEffectParams","CompleteCallback",false);
    const auto flags_field=s.Get("TriggerEffectParams","Flags",false);
    auto* flags=s.FlagArray(6); // ordinary projectile hit helper adds MonoEffectFlag 6
    out.target=s.Set(caster_field,parameters,caster);
    if(out.target.returned)out.target=s.Set(param_field,parameters,target);
    if(out.target.returned)out.target=s.Set(target_field,parameters,options.attach?target:nullptr);
    if(out.target.returned)out.target=s.Set(flags_field,parameters,flags);
    if(out.target.returned) {
        struct NullableVector {Position3 value;std::uint8_t present;std::uint8_t padding[3]{};};
        struct NullableRotation {Rotation4 value;std::uint8_t present;std::uint8_t padding[3]{};};
        static_assert(sizeof(NullableVector)==16 && sizeof(NullableRotation)==20);
        // Same normal-hit inputs: no inferred attach point or position offset,
        // and no generic entity-relative/reset flags layered on top of them.
        s.Scalar(parameters,"InitPosition",NullableVector{options.position,1});
        s.Scalar(parameters,"InitRotation",NullableRotation{rotation,1});
        s.Scalar(parameters,"ParamEntityUsage",std::int32_t{5});
        out.completion=s.Set(callback_field,parameters,completion);
    }
    return out;
}
EffectInputResult EffectInputs::ConfigureImpl(void* parameters,void* caster,void* target,void* completion,
    const EntityEffectOptions& options,const SpatialEffectOptions* spatial) {
    auto& s=*state_;EffectInputResult out;
    if(s.closed || !parameters || !caster || !completion)return out;
    const auto finite=[](Position3 value){return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);};
    if(!finite(options.position_offset) || !finite(options.rotation_offset) || (options.scale && !finite(*options.scale)) ||
        options.unique_name.find('\0')!=std::string::npos || options.attach_point.find('\0')!=std::string::npos)
        throw std::invalid_argument("invalid entity effect visual inputs");
    if(spatial)s.PrepareSpatial(parameters);else s.PrepareEntity(parameters);
    const auto caster_field=s.Get("TriggerEffectParams","CasterEntity",false);
    const auto target_field=s.Get("TriggerEffectParams","TargetEntity",false);
    const auto name_field=s.Get("TriggerEffectParams","UniqueEffectName",false);
    const auto callback_field=s.Get("TriggerEffectParams","CompleteCallback",false);
    const auto attach_field=s.Get("TriggerEffectParams","DynamicAttach",false);
    const auto flags_field=s.Get("TriggerEffectParams","Flags",false);
    auto* flags=options.resident?s.FlagArray(1):nullptr;
    ManagedRootId text_root=0,attach_root=0;
    try {
        auto* name=s.Keep(s.objects.NewStringUtf8(options.unique_name.c_str()));text_root=s.retained.back();
        void* attach_point=nullptr;
        if(!options.attach_point.empty()) {
            attach_point=s.Keep(s.objects.NewStringUtf8(options.attach_point.c_str()));attach_root=s.retained.back();
        }
        out.target=s.Set(caster_field,parameters,caster);
        if(out.target.returned)out.target=s.Set(target_field,parameters,target);
        if(out.target.returned)out.target=s.Set(name_field,parameters,name);
        // The official task copies TriggerEffect.AttachPoint to DynamicAttach.
        // The loader derives AttachTransform/AttachPointName before Reset;
        // writing those cached outputs would be overwritten on load.
        if(out.target.returned)out.target=s.Set(attach_field,parameters,attach_point);
        if(out.target.returned)out.target=s.Set(flags_field,parameters,flags);
        if(out.target.returned) {
            // Qualified HSR nullable Vector3 storage: value at +0, presence at
            // +12. Source vectors and parameter offsets are distinct contracts.
            struct NullableVector {Position3 value;std::uint8_t present;std::uint8_t padding[3]{};};
            static_assert(sizeof(NullableVector)==16 && offsetof(NullableVector,present)==12);
            const auto relative=static_cast<std::uint8_t>(!spatial);
            const NullableVector position{options.position_offset,relative},rotation{options.rotation_offset,relative};
            const NullableVector scale{options.scale.value_or(Position3{}),static_cast<std::uint8_t>(options.scale.has_value())};
            s.Scalar(parameters,"PositionOffset",position);s.Scalar(parameters,"RotationOffset",rotation);s.Scalar(parameters,"CustomScale",scale);
            s.Scalar(parameters,"IsNeedFadeIn",static_cast<std::uint8_t>(options.fade_in));
            s.Scalar(parameters,"ForceSimulateImmediately",static_cast<std::uint8_t>(options.simulate_immediately));
            s.Scalar(parameters,"IsEnableReplay",std::uint8_t{0});
            // Original task default is LookAt. ParamEntity stays null: no
            // toward evaluator or corresponding completion processing implied.
            s.Scalar(parameters,"ParamEntityUsage",std::int32_t{1});
            s.Scalar(parameters,"TowardMaxPitchAngle",90.0f);
            if(spatial) {
                // The loader consumes InitPosition before ResetTransform uses
                // AnchorPosition. Populate both, without a target/attachment.
                const NullableVector world{spatial->position,1};
                struct NullableRotation {Rotation4 value;std::uint8_t present;std::uint8_t padding[3]{};};
                static_assert(sizeof(NullableRotation)==20 && offsetof(NullableRotation,present)==16);
                const NullableRotation orientation{spatial->rotation,1};
                s.Scalar(parameters,"InitPosition",world);s.Scalar(parameters,"AnchorPosition",world);
                s.Scalar(parameters,"InitRotation",orientation);
                s.Scalar(parameters,"IgnorePrefabAttachPoint",std::uint8_t{1});
            }
            out.target=native::CallVoid(s.attach,parameters,options.attach_to_target);
        }
        if(out.target.returned)out.completion=s.Set(callback_field,parameters,completion);
    }catch(...){s.ReleaseValue(attach_root);s.ReleaseValue(text_root);throw;}
    s.ReleaseValue(attach_root);s.ReleaseValue(text_root);return out;
}
bool EffectInputs::Close() {
    auto& s=*state_;s.closed=true;s.fields.clear();
    for(auto it=s.retained.begin();it!=s.retained.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.retained.erase(it);}else ++it;
    }
    return s.retained.empty();
}
}
