#pragma once
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/managed/ManagedObjects.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <memory>

namespace hksr::sdk_v1::detail {
struct PartnerEventMethods {
    Method component,owner_world,delegate_ctor,callback,unregister;
    std::size_t component_owner=0,world_events=0;
    std::int32_t event=66;
    std::uint32_t filter=1;
};
struct PartnerEventCleanupResult {
    bool returned=false,unregister_attempted=false;
    CallResult call;
    std::string error;
};
// Internal companion to EntityCreationSession. Called on the owner thread,
// before components are disposed, including disposal inside the factory.
class HKSR_API PartnerEventCleanup {
public:
    PartnerEventCleanup(const EntityOwner&,PartnerEventMethods,ReferenceMethods,ManagedObjects&);
    ~PartnerEventCleanup();
    PartnerEventCleanup(const PartnerEventCleanup&)=delete;
    PartnerEventCleanup& operator=(const PartnerEventCleanup&)=delete;
    PartnerEventCleanupResult Remove(EntityOrigin);
    bool Close();
private:
    struct State;
    State* state_;
};
HKSR_API PartnerEventMethods ResolvePartnerEventCleanup460(MethodResolver&,
    const std::vector<DeclaredField>& components,const std::vector<DeclaredField>& world);
}
