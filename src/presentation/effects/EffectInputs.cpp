#include <hksr/presentation/effects/EffectInputs.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct EffectInputs::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;ManagedRoots roots;
    std::vector<ManagedRootId> retained;
    struct Field {void* object=nullptr;Method call;};
    std::map<std::string,Field> fields;
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
};
EffectInputs::EffectInputs(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r):state_(new State(m,d,o,r)){}
EffectInputs::~EffectInputs(){delete state_;}
ObjectResult EffectInputs::Manager() {
    if(state_->closed)return {};
    const auto field=state_->Get("GlobalVars","s_MonoEffectManager",true);
    ObjectResult out;out.call=native::Call(field.call,&out.object,field.object,static_cast<void*>(nullptr));return out;
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
bool EffectInputs::Close() {
    auto& s=*state_;s.closed=true;s.fields.clear();
    for(auto it=s.retained.begin();it!=s.retained.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.retained.erase(it);}else ++it;
    }
    return s.retained.empty();
}
}
