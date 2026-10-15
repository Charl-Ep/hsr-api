#include "ManagedConfigProjection.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
namespace {
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool IsArray(const DeclaredType& type) {
    return type.name.size()>2 && type.name.compare(type.name.size()-2,2,"[]")==0;
}
}
struct ManagedConfigProjection::State {
    MethodResolver& methods; ObjectMetadata& metadata; ManagedObjects& objects;
    ManagedRoots roots; ManagedArrays arrays; ConfigShapeReader shape;
    Method clone;
    std::string operation="retain projection inputs";
    bool attempted=false,closing=false;
    std::vector<ManagedRootId> held;
    std::set<void*> retained,copies;
    struct Field {void* object; Method get,set; std::string label;};
    struct Node {bool array=false; ConfigObjectShape shape; std::vector<Field*> edges; Field* enabled=nullptr;};
    std::map<std::string,Field> fields;
    std::map<void*,Node> nodes;
    void* false_value=nullptr;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,ArrayMethods a,ConfigShapeReader s)
        :methods(m),metadata(d),objects(o),roots(r),arrays(std::move(a)),shape(std::move(s)),
        clone(m.Resolve("System","Object","MemberwiseClone","System.Object",{})) {}
    void* Keep(void* object) {
        if(!object || retained.count(object)) return object;
        // Allocate the receipt before retaining; a failure remains owned.
        held.emplace_back(); auto& id=held.back(); id=roots.Keep(object);
        Require(roots.Status(id).ready,"configuration GC retention failed");
        retained.insert(object); return object;
    }
    void* Object(ObjectResult result) {
        if(!result.call.returned)throw std::runtime_error(operation+": managed call failed; exception="+std::to_string(result.call.exception));
        return Keep(result.object);
    }
    template<class... Args> void* Invoke(Method method,Args... args) {
        ObjectResult result; result.call=native::Call(method,&result.object,args...); return Object(result);
    }
    Field& Resolve(const DeclaredField& selected) {
        Require(selected.flags_observed && !(selected.flags&0x10),"configuration field must be a declared instance field");
        const auto key=selected.owner_namespace+"."+selected.owner_name+"."+selected.name;
        operation="resolve configuration field "+key;
        const auto found=fields.find(key); if(found!=fields.end()) return found->second;
        auto* type=Object(objects.ManagedType(selected.owner_namespace.c_str(),selected.owner_name.c_str()));
        auto* name=Object(objects.NewStringUtf8(selected.name.c_str()));
        Require(type && name,"configuration field identity absent");
        // DeclaredOnly avoids a same-named field on a derived/parent class.
        auto* field=Invoke(methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"}),type,name,54);
        Require(field!=nullptr,"configuration reflection field absent");
        const auto actual=metadata.ReadObjectType(field); MetadataResolver resolver({actual});
        Field value{field,
            resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetValue","System.Object",{"System.Object"}),
            resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
                {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"}),key};
        return fields.emplace(key,value).first->second;
    }
    void Set(Field& field,void* target,void* value) {
        operation="write private configuration field "+field.label;
        const auto result=native::CallVoid(field.set,field.object,target,value,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr));
        if(!result.returned)throw std::runtime_error(operation+": exception="+std::to_string(result.exception));
    }
    void* FalseValue() {
        if(false_value) return false_value;
        operation="allocate boxed false value";
        auto* value=Object(objects.AllocateObject("System","Boolean"));
        Require(value!=nullptr,"configuration false value allocation failed");
        const auto type=metadata.ReadObjectType(value);
        Require(type.name_space=="System" && type.name=="Boolean","configuration false value type differs");
        const auto members=metadata.ReadObjectFields(value);
        unsigned matches=0; std::uint8_t zero=1;
        for(const auto& field:members) if(field.name=="m_value" && field.type=="System.Boolean" && field.flags_observed && !(field.flags&0x10)) {
            ++matches; Require(native::CopyMemoryChecked(&zero,static_cast<unsigned char*>(value)+field.offset,sizeof(zero)),"configuration Boolean read failed");
        }
        Require(matches==1 && zero==0,"configuration Boolean is not zero-initialized");
        false_value=value; return value;
    }
};
ManagedConfigProjection::ManagedConfigProjection(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,
    ReferenceMethods r,ArrayMethods a,ConfigShapeReader s):state_(std::make_unique<State>(m,d,o,r,std::move(a),std::move(s))) {}
ManagedConfigProjection::~ManagedConfigProjection()=default;
ConfigProjection ManagedConfigProjection::Apply(const std::vector<void*>& input) {
    auto& s=*state_; Require(!s.attempted && !s.closing,"configuration projection is single-use");s.attempted=true;
    // Retain every top-level input before any allocation/reflection traversal.
    for(auto* root:input) s.Keep(root);
    return ProjectConfig(input,*this);
}
ConfigView ManagedConfigProjection::Inspect(void* source) {
    auto& s=*state_;s.Keep(source);const auto type=s.metadata.ReadObjectType(source);
    auto& node=s.nodes[source];node.array=IsArray(type);ConfigView result;
    if(node.array) {
        const auto count=s.arrays.Length(source);Require(count.read.returned && count.value>=0,"configuration array length unavailable");
        for(int i=0;i<count.value;++i) {
            s.operation="read "+type.name+"["+std::to_string(i)+"]";
            result.edges.push_back({static_cast<std::size_t>(i),s.Object(s.arrays.Get(source,i))});
        }
    } else {
        node.shape=s.shape(source,type);
        for(const auto& field:node.shape.edges) {
            auto& access=s.Resolve(field);node.edges.push_back(&access);
            s.operation="read configuration field "+access.label;
            result.edges.push_back({node.edges.size()-1,s.Invoke(access.get,access.object,source)});
        }
        if(node.shape.disable_task) {
            const auto& enabled=node.shape.enabled;
            Require(enabled.type=="System.Boolean","configuration task enable field is not Boolean");
            node.enabled=&s.Resolve(enabled);
            std::uint8_t value=0;
            Require(native::CopyMemoryChecked(&value,static_cast<unsigned char*>(source)+enabled.offset,sizeof(value)),"configuration task enable read failed");
            result.disable_task=value!=0;
        }
    }
    return result;
}
void* ManagedConfigProjection::Copy(void* source) {
    auto& s=*state_;const auto before=s.metadata.ReadObjectType(source);
    s.operation="clone configuration "+before.name_space+"."+before.name;
    auto* copy=s.Invoke(s.clone,source);
    Require(copy && !s.nodes.count(copy) && s.copies.insert(copy).second,"configuration clone identity invalid");
    const auto after=s.metadata.ReadObjectType(copy);
    Require(before.name_space==after.name_space && before.name==after.name,"configuration clone type differs");
    s.nodes.emplace(copy,s.nodes.at(source));return copy;
}
void ManagedConfigProjection::SetEdge(void* copy,std::size_t slot,void* target) {
    auto& s=*state_;Require(s.copies.count(copy)!=0,"configuration writes require a private copy");
    const auto& node=s.nodes.at(copy);
    if(node.array) Require(s.arrays.Set(copy,target,static_cast<std::int32_t>(slot)).returned,"configuration private array write failed");
    else s.Set(*node.edges.at(slot),copy,target);
}
void ManagedConfigProjection::DisableTask(void* copy) {
    auto& s=*state_;Require(s.copies.count(copy)!=0,"configuration disable requires a private copy");
    auto* field=s.nodes.at(copy).enabled;Require(field!=nullptr,"configuration enabled field absent");s.Set(*field,copy,s.FalseValue());
    std::uint8_t enabled=1;
    const auto offset=s.nodes.at(copy).shape.enabled.offset;
    Require(native::CopyMemoryChecked(&enabled,static_cast<unsigned char*>(copy)+offset,sizeof(enabled)) && enabled==0,
        "private configuration did not adopt TaskEnabled=false");
}
bool ManagedConfigProjection::Close() {
    auto& s=*state_;s.closing=true;
    for(auto it=s.held.begin();it!=s.held.end();) {
        if(!*it || s.roots.Release(*it).complete) {if(*it)s.roots.Forget(*it);it=s.held.erase(it);}else ++it;
    }
    return s.held.empty();
}
}
