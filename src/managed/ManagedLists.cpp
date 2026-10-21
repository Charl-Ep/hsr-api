#include <hksr/managed/ManagedLists.hpp>
#include <hksr/native/Invocation.hpp>
#include "../metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
ManagedReferenceList::ManagedReferenceList(ReferenceListMethods methods):methods_(std::make_shared<const ReferenceListMethods>(std::move(methods))){}
ManagedReferenceList::~ManagedReferenceList()=default;
ManagedReferenceList::ManagedReferenceList(const ManagedReferenceList&) noexcept=default;
ManagedReferenceList& ManagedReferenceList::operator=(const ManagedReferenceList&) noexcept=default;

Observed<std::int32_t> ManagedReferenceList::Count(void* list) const {
    Observed<std::int32_t> out;
    if(list) out.read=native::Call(methods_->count,&out.value,list);
    return out;
}
ObjectResult ManagedReferenceList::Item(void* list,std::int32_t index) const {
    ObjectResult out;
    if(list && index>=0) out.call=native::Call(methods_->item,&out.object,list,index);
    return out;
}
ReferenceListMethods ResolveReferenceListMethods(const DeclaredType& type,const char* element_type) {
    MetadataResolver source({type}); detail::ResolveBatch b(source);
    ReferenceListMethods out;
    out.count=b.Resolve(type.name_space.c_str(),type.name.c_str(),"get_Count","System.Int32",{});
    out.item=b.Resolve(type.name_space.c_str(),type.name.c_str(),"get_Item",element_type,{"System.Int32"});
    out.binding_errors=b.Errors(); return out;
}
ReferenceListMethods ResolveReturnedReferenceList(RuntimeMetadata& metadata,Method getter,const char* element_type) {
    return ResolveReferenceListMethods(metadata.ReadType(metadata.ReturnType(getter)),element_type);
}
ReferenceListMethods ResolveReferenceListHierarchy(const std::vector<DeclaredType>& types,const char* element_type) {
    ReferenceListMethods out;
    const auto find=[&](const char* name,const char* result,std::initializer_list<const char*> parameters) {
        const std::vector<std::string> expected(parameters.begin(),parameters.end());
        for(const auto& type:types)for(const auto& method:type.methods)
            if(method.name==name && method.result==result && method.parameters==expected && !method.is_static) {
                try {MetadataResolver resolver({type});return resolver.Resolve(type.name_space.c_str(),type.name.c_str(),name,result,parameters);}
                catch(const std::exception& e){out.binding_errors.push_back(e.what());return Method{};}
            }
        out.binding_errors.push_back(std::string("reference list hierarchy has no ")+name);return Method{};
    };
    out.count=find("get_Count","System.Int32",{});
    out.item=find("get_Item",element_type,{"System.Int32"});
    return out;
}
ReferenceListMethods ResolveReferenceListHierarchy(RuntimeMetadata& metadata,RuntimeType type,const char* element_type) {
    std::vector<DeclaredType> types;std::vector<RuntimeType> visited;
    while(type) {
        for(auto previous:visited)if(previous==type)throw std::runtime_error("reference list parent cycle");
        visited.push_back(type);types.push_back(metadata.ReadType(type));type=metadata.ParentType(type);
    }
    return ResolveReferenceListHierarchy(types,element_type);
}
}
