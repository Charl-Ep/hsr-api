#pragma once
#include <memory>
#include "../metadata/RuntimeMetadata.hpp"

namespace hksr::sdk_v1 {
struct ProfileInputMethods {
    Method model_path,entity_config,operation_constructor,animator_initialized;
    std::vector<std::string> binding_errors;
};
struct ProfileEventFields {
    std::size_t paths=0,layers=0;
    bool has_paths=false,has_layers=false;
    std::vector<std::string> binding_errors;
};
struct ProfileEventArrays { ObjectResult paths,layers; };
// Version names/layout selection remain inside the SDK. Unrooted results must
// be retained by the caller before constructing or submitting an operation.
class ProfileInputs {
public:
    HKSR_API explicit ProfileInputs(ProfileInputMethods methods);
    HKSR_API ~ProfileInputs();
    HKSR_API ProfileInputs(const ProfileInputs&) noexcept;
    HKSR_API ProfileInputs& operator=(const ProfileInputs&) noexcept;
    HKSR_API ObjectResult ModelPath(void* rooted_model) const;
    HKSR_API ObjectResult EntityConfig(void* rooted_model) const;
    HKSR_API ObjectResult AllocateOperation(ManagedObjects& objects) const;
    HKSR_API CallResult InitializeOperation(void* rooted_operation) const;
    HKSR_API Observed<bool> AnimatorInitialized(void* rooted_animator) const;
private:
    std::shared_ptr<const ProfileInputMethods> methods_;
};
HKSR_API ProfileInputMethods ResolveProfileInputMethods(MethodResolver& resolver);
// Input is the actual event-consumer's current hierarchy declarations, not
// fields from the outer event component or a cached offline address report.
HKSR_API ProfileEventFields ResolveProfileEventFields(const std::vector<DeclaredField>& fields);
HKSR_API ProfileEventArrays ReadProfileEventArrays(void* rooted_consumer,ProfileEventFields fields);
}
