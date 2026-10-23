#include <hksr/presentation/projectiles/ProjectileInputs.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ProjectileInputs::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ManagedRoots roots;std::vector<ManagedRootId> held;std::set<void*> retained;
    ProjectileInputStatus status;bool attempted=false;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r):methods(m),metadata(d),objects(o),roots(r){}
    void* Keep(void* value) {
        if(!value || retained.count(value))return value;
        held.reserve(held.size()+1);held.push_back(roots.Keep(value));
        if(!roots.Status(held.back()).ready)throw std::runtime_error("projectile input retention failed");
        retained.insert(value);return value;
    }
    void* Object(ObjectResult r) {
        if(!r.call.returned || !r.object)throw std::runtime_error("projectile input allocation failed");
        return Keep(r.object);
    }
    template<class... A>void* Invoke(Method m,A... args) {
        ObjectResult r;r.call=native::Call(m,&r.object,args...);return Object(r);
    }
    template<class T>T Read(void* p,std::size_t offset) {
        T out{};
        if(!p || !native::CopyMemoryChecked(&out,static_cast<char*>(p)+offset,sizeof(out)))
            throw std::runtime_error("projectile input read failed");
        return out;
    }
    DeclaredField Field(void* p,const std::string& name,const char* type) {
        std::optional<DeclaredField> selected;
        for(const auto& f:metadata.ReadObjectFields(p))if(f.name==name) {
            if(selected || f.type!=type || !f.flags_observed || (f.flags&0x10))
                throw std::runtime_error("projectile input field differs: "+name);
            selected=f;
        }
        if(!selected)throw std::runtime_error("projectile input field absent: "+name);
        return *selected;
    }
    void Reference(void* copy,const DeclaredField& f,void* value) {
        auto* type=Object(objects.ManagedType(f.owner_namespace.c_str(),f.owner_name.c_str()));
        auto* name=Object(objects.NewStringUtf8(f.name.c_str()));
        auto* field=Invoke(methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"}),type,name,54);
        const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
        const auto set=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
        if(!native::CallVoid(set,field,copy,value,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)).returned)
            throw std::runtime_error("projectile private reference write failed: "+f.name);
    }
    template<class T>void Scalar(void* copy,const char* name,const char* type,T value) {
        const auto f=Field(copy,name,type);
        if(!native::CopyMemoryChecked(static_cast<char*>(copy)+f.offset,&value,sizeof(value)))
            throw std::runtime_error(std::string("projectile private value write failed: ")+name);
    }
};
ProjectileInputs::ProjectileInputs(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r)
    :state_(std::make_unique<State>(m,d,o,r)){}
ProjectileInputs::~ProjectileInputs()=default;
const ProjectileInputStatus& ProjectileInputs::Status()const{return state_->status;}
bool ProjectileInputs::Prepare(void* source,const std::map<std::string,float>& values) {
    auto& s=*state_;auto& out=s.status;if(s.attempted || out.closed)return false;s.attempted=true;
    try {
        if(!source)throw std::invalid_argument("projectile source absent");
        s.Keep(source);const auto type=s.metadata.ReadObjectTypeName(source);
        if(type.name_space!="RPG.GameCore")throw std::invalid_argument("projectile configuration namespace differs");
        out.legacy=type.name=="ProjectileData";
        if(out.legacy) {
            const auto behavior=s.Field(source,"Behavior","RPG.GameCore.ProjectileMoveBehavior");
            const auto value=s.Read<int>(source,behavior.offset);
            if(value!=1 && value!=2 && value!=4 && value!=7)throw std::invalid_argument("projectile behavior is not qualified");
            out.family=static_cast<ProjectileFamily>(value);
        } else if(type.name=="LinearProjectileData")out.family=ProjectileFamily::Linear;
        else if(type.name=="RandomTraceProjectileData")out.family=ProjectileFamily::RandomTrace;
        else throw std::invalid_argument("projectile configuration type is not qualified");

        std::vector<DeclaredField> numeric;
        std::set<std::string> used;
        std::set<std::string> required{"FlySpeed","FlyTime"};
        if(out.family==ProjectileFamily::Linear)required.insert("LinearPitchAngle");
        if(out.family==ProjectileFamily::HalfBoomerang) {
            required.insert("BoomerangEccentricity");required.insert("BoomerangAngleRoll");
        }
        if(out.family==ProjectileFamily::PrefabCurve)required.insert("SubCurveIndex");
        for(const auto& f:s.metadata.ReadObjectFields(source))if(f.type=="RPG.GameCore.DynamicFloat" && f.flags_observed && !(f.flags&0x10)) {
            if(!used.insert(f.name).second)throw std::runtime_error("ambiguous projectile numeric field: "+f.name);
            auto* dynamic=s.Read<void*>(source,f.offset);s.Keep(dynamic);
            const auto replacement=values.find(f.name);
            if(replacement!=values.end()) {
                if(!std::isfinite(replacement->second))throw std::invalid_argument("non-finite projectile value: "+f.name);
                numeric.push_back(f);
            } else if(dynamic && required.count(f.name)) {
                const auto expr=s.Field(dynamic,"postfixExpr","JJAABNBJMEH");
                if(s.Read<void*>(dynamic,expr.offset))out.unresolved_values.push_back(f.name);
            }
        }
        for(const auto& v:values)if(!used.count(v.first))throw std::invalid_argument("unknown projectile value: "+v.first);
        if(!out.unresolved_values.empty())throw std::invalid_argument("projectile expressions require evaluated inputs");
        const auto clone=s.methods.Resolve("System","Object","MemberwiseClone","System.Object",{});
        auto* copy=s.Invoke(clone,source);
        if(copy==source)throw std::runtime_error("projectile clone aliases its source");
        const auto copied=s.metadata.ReadObjectTypeName(copy);
        if(copied.name_space!=type.name_space || copied.name!=type.name)throw std::runtime_error("projectile clone type differs");
        if(!numeric.empty()) {
            const auto convert=s.methods.Resolve("RPG.GameCore","DynamicFloat","BIIMDAFMJNF","RPG.GameCore.DynamicFloat",{"System.Single"},MethodKind::Static);
            for(const auto& f:numeric)s.Reference(copy,f,s.Invoke(convert,values.at(f.name)));
        }
        const auto read_bool=[&](const char* name){return s.Read<bool>(copy,s.Field(copy,name,"System.Boolean").offset);};
        const auto hit=s.Field(copy,"HitEffect","System.String");
        auto* fly=s.Read<void*>(copy,s.Field(copy,"FlyEffect","System.String").offset);
        if(fly) {
            s.Keep(fly);int length=0;
            const auto r=native::Call(s.methods.Resolve("System","String","get_Length","System.Int32",{}),&length,fly);
            if(!r.returned || length<0)throw std::runtime_error("projectile FlyEffect name unreadable");
            out.expects_effect=length!=0;
        }
        out.hit_effect=s.Keep(s.Read<void*>(copy,hit.offset));
        out.align_hit=read_bool("AlignHitEffect");out.attach_hit=read_bool("AttachHitEffectToTargetEntity");
        out.start_from_last_hit=read_bool("StartFromLastHitPos");out.use_last_velocity=read_bool("UseLastHitVelocity");
        // References use reflection's GC barrier; only private blittable bools
        // use direct stores. No mutation of shared parameter/curve children.
        s.Reference(copy,hit,nullptr);
        s.Reference(copy,s.Field(copy,"FlyEffectUniqueName","System.String"),nullptr);
        s.Scalar(copy,"TriggerHitCallback","System.Boolean",false);
        s.Scalar(copy,"EnableRayCast","System.Boolean",false);
        s.Scalar(copy,"CorrectedWhenTargetFormationChange","System.Boolean",false);
        out.parameters=copy;out.prepared=true;return true;
    }catch(const std::exception& e){out.error=e.what();Close();return false;}
}
bool ProjectileInputs::Close() {
    auto& s=*state_;auto& out=s.status;
    out.prepared=false;out.parameters=nullptr;out.hit_effect=nullptr;
    for(auto it=s.held.begin();it!=s.held.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.held.erase(it);}else ++it;
    }
    out.closed=s.held.empty();return out.closed;
}
}
