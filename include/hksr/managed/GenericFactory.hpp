#pragma once
#include "ManagedArrays.hpp"
#include "ManagedObjects.hpp"
#include "ManagedRoots.hpp"

namespace hksr::sdk_v1 {
struct GenericFactoryMethods {Method find,definition,arguments,close,open,invoke;};
HKSR_API GenericFactoryMethods ResolveGenericFactoryMethods(MethodResolver&);
// Closed, parameterless instance factory with one generic type argument.
// The caller qualifies its signature, owns receiver lifetime and rolls back
// partial construction. Prepare caches rooted reflection inputs; Invoke never
// searches the domain. Engine-attached calls, explicit GC cleanup via Close.
class HKSR_API GenericFactory {
public:
    GenericFactory(GenericFactoryMethods,ReferenceMethods,ManagedObjects&,ManagedArrays);
    ~GenericFactory(); // no engine calls
    GenericFactory(const GenericFactory&)=delete;
    GenericFactory& operator=(const GenericFactory&)=delete;
    bool Prepare(void* declaring_type,const char* method,void* argument_type);
    ObjectResult Invoke(void* receiver) const;
    bool Close();
    const std::string& Error() const;
private:
    struct State;State* state_;
};
}
