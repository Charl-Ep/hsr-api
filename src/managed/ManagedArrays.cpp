#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::sdk_v1 {
ManagedArrays::ManagedArrays(ArrayMethods methods):methods_(std::make_shared<const ArrayMethods>(std::move(methods))){}
ManagedArrays::~ManagedArrays()=default;
ManagedArrays::ManagedArrays(const ManagedArrays&) noexcept=default;
ManagedArrays& ManagedArrays::operator=(const ManagedArrays&) noexcept=default;

HierarchyAccess::HierarchyAccess(HierarchyMethods methods):methods_(std::make_shared<const HierarchyMethods>(std::move(methods))){}
HierarchyAccess::~HierarchyAccess()=default;
HierarchyAccess::HierarchyAccess(const HierarchyAccess&) noexcept=default;
HierarchyAccess& HierarchyAccess::operator=(const HierarchyAccess&) noexcept=default;

ObjectResult ManagedArrays::Create(void* type,std::int32_t count) const {
    ObjectResult r; r.call=native::Call(methods_->create,&r.object,type,count); return r;
}
Observed<std::int32_t> ManagedArrays::Length(void* array) const {
    Observed<std::int32_t> r; r.read=native::Call(methods_->length,&r.value,array); return r;
}
ObjectResult ManagedArrays::Get(void* array,std::int32_t index) const {
    ObjectResult r; r.call=native::Call(methods_->get,&r.object,array,index); return r;
}
CallResult ManagedArrays::Set(void* array,void* value,std::int32_t index) const { return native::CallVoid(methods_->set,array,value,index); }
ObjectResult HierarchyAccess::Find(void* root,void* path) const {
    ObjectResult r; r.call=native::Call(methods_->find,&r.object,root,path); return r;
}
}
