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
}
