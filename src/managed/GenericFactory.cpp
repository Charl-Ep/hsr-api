#include <hksr/managed/GenericFactory.hpp>
#include <hksr/native/Invocation.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct GenericFactory::State {
    GenericFactoryMethods methods;ManagedObjects& objects;ManagedArrays arrays;
    ManagedRoots roots;std::vector<ManagedRootId> handles;
    void* factory=nullptr;void* declaring=nullptr;void* argument=nullptr;
    bool attempted=false,closed=false;std::string error,name;
    State(GenericFactoryMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays a)
        :methods(m),objects(o),arrays(std::move(a)),roots(r){}
    void* Keep(void* object) {
        if(!object)throw std::runtime_error("generic factory reference absent");
        handles.reserve(handles.size()+1);handles.push_back(roots.Keep(object));
        if(!roots.Status(handles.back()).ready)throw std::runtime_error("generic factory reference retention failed");
        return object;
    }
    void* Keep(ObjectResult value) {
        if(!value.call.returned)throw std::runtime_error("generic factory reflection call failed");
        return Keep(value.object);
    }
};
GenericFactory::GenericFactory(GenericFactoryMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays a)
    :state_(new State(m,r,o,std::move(a))){}
GenericFactory::~GenericFactory(){delete state_;}
bool GenericFactory::Prepare(void* declaring,const char* name,void* argument) {
    auto& s=*state_;
    if(s.attempted || s.closed)return s.factory && !s.closed && s.declaring==declaring &&
        s.argument==argument && name && s.name==name;
    s.attempted=true;
    try {
        if(!name || !*name)throw std::invalid_argument("generic factory name absent");
        s.declaring=declaring;s.argument=argument;s.name=name;
        s.Keep(declaring);s.Keep(argument);
        auto* text=s.Keep(s.objects.NewStringUtf8(name));
        ObjectResult definition;
        definition.call=native::Call(s.methods.find,&definition.object,declaring,text);
        auto* method=s.Keep(definition);bool is_definition=false;
        if(!native::Call(s.methods.definition,&is_definition,method).returned || !is_definition)
            throw std::runtime_error("factory is not a generic method definition");
        ObjectResult parameters;parameters.call=native::Call(s.methods.arguments,&parameters.object,method);
        auto* arguments=s.Keep(parameters);const auto count=s.arrays.Length(arguments);
        if(!count.read.returned || count.value!=1)throw std::runtime_error("factory requires exactly one generic argument");
        auto* type=s.Keep(s.objects.ManagedType("System","Type"));
        auto* input=s.Keep(s.arrays.Create(type,1));
        if(!s.arrays.Set(input,argument,0).returned)throw std::runtime_error("generic argument assignment failed");
        ObjectResult result;result.call=native::Call(s.methods.close,&result.object,method,input);
        auto* factory=s.Keep(result);bool open=true;
        if(!native::Call(s.methods.open,&open,factory).returned || open)
            throw std::runtime_error("factory still has open generic parameters");
        s.factory=factory;return true;
    }catch(const std::exception& e){s.error=e.what();return false;}
}
ObjectResult GenericFactory::Invoke(void* receiver) const {
    const auto& s=*state_;ObjectResult out;
    if(!s.closed && s.factory && receiver)
        out.call=native::Call(s.methods.invoke,&out.object,s.factory,receiver,static_cast<void*>(nullptr));
    return out;
}
bool GenericFactory::Close() {
    auto& s=*state_;s.closed=true;s.factory=nullptr;
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.handles.erase(it);}else ++it;
    }
    return s.handles.empty();
}
const std::string& GenericFactory::Error()const{return state_->error;}
}
