#pragma once
#include <memory>
#include "../metadata/MethodResolver.hpp"

namespace hksr::sdk_v1 {
// These references must belong to this instance and remain rooted by the SDK
// caller for the complete official operation. No ownership is transferred here.
struct ModelInputs {
    void* model_path=nullptr;
    void* adventure_config=nullptr;
    void* event_paths=nullptr;
    void* excluded_layers=nullptr;
};
static_assert(sizeof(ModelInputs)==4*sizeof(void*));
struct ChangeResult { CallResult call; bool changed=false; };
struct ModelMethods { Method begin,cancel,change,events,can_change,restore; };

// First extracted SDK capability, not a complete runtime or automatic bootstrap.
// Version-specific names are supplied by the adapter implementation, not mods.
class ModelProfile {
public:
    HKSR_API explicit ModelProfile(ModelMethods methods);
    HKSR_API ModelProfile(ModelMethods methods,Method pending);
    HKSR_API ~ModelProfile();
    HKSR_API ModelProfile(const ModelProfile&) noexcept;
    HKSR_API ModelProfile& operator=(const ModelProfile&) noexcept;
    HKSR_API CallResult Begin(void* operation,void* entity,ModelInputs inputs,bool asynchronous,void* callback) const;
    HKSR_API CallResult Cancel(void* operation) const;
    HKSR_API bool HasAsyncCompletion() const;
    HKSR_API Observed<bool> Pending(void* operation) const;
    HKSR_API ChangeResult Change(void* model,void* path,void* prepared_root) const;
    HKSR_API ChangeResult CanChange(void* model,void* path,void** resolved_path) const;
    HKSR_API CallResult ReplaceEvents(void* component,void* paths,void* excluded_layers) const;
    HKSR_API CallResult Restore(void* model,void* root_path,void* entity_config) const;
private:
    struct State;
    std::shared_ptr<const State> methods_;
};
// Locks ONLY the declared method contract. Not a version authorization function.
HKSR_API ModelMethods ResolveModelMethods(MethodResolver& resolver);
HKSR_API Method ResolveModelPending(MethodResolver& resolver);
}
