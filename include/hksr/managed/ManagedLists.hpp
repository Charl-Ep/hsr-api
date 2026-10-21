#pragma once
#include <memory>
#include "../metadata/RuntimeMetadata.hpp"
namespace hksr::sdk_v1 {
struct ReferenceListMethods {
    Method count,item;
    std::vector<std::string> binding_errors;
};
// Read-only constructed generic list of managed references, not value types.
// Inputs and returned objects must be rooted by the caller when retained.
// No guessed array/header/version layout and no list mutation here.
class ManagedReferenceList {
public:
    HKSR_API explicit ManagedReferenceList(ReferenceListMethods methods);
    HKSR_API ~ManagedReferenceList();
    HKSR_API ManagedReferenceList(const ManagedReferenceList&) noexcept;
    HKSR_API ManagedReferenceList& operator=(const ManagedReferenceList&) noexcept;
    HKSR_API Observed<std::int32_t> Count(void* list) const;
    HKSR_API ObjectResult Item(void* list,std::int32_t index) const;
private: std::shared_ptr<const ReferenceListMethods> methods_;
};
// Use the exact constructed return type of the official list getter, not a
// domain-wide search for List`1 or a similarly named generic definition.
HKSR_API ReferenceListMethods ResolveReferenceListMethods(const DeclaredType& constructed_type,const char* element_type);
// Derived-first declarations retain each closed generic owner's MethodInfo.
HKSR_API ReferenceListMethods ResolveReferenceListHierarchy(const std::vector<DeclaredType>&,const char* element_type);
HKSR_API ReferenceListMethods ResolveReferenceListHierarchy(RuntimeMetadata&,RuntimeType,const char* element_type);
HKSR_API ReferenceListMethods ResolveReturnedReferenceList(RuntimeMetadata& metadata,Method getter,const char* element_type);
}
