#pragma once
#include <memory>
#include "../metadata/MethodResolver.hpp"
#include <cstdint>

namespace hksr::sdk_v1 {
struct ReferenceMethods { Method retain,get,free,load_asset,release_asset; };
struct RootResult { CallResult call; std::int32_t handle=0; };
struct ObjectResult { CallResult call; void* object=nullptr; };
class References {
public:
    HKSR_API explicit References(ReferenceMethods methods);
    HKSR_API ~References();
    HKSR_API References(const References&) noexcept;
    HKSR_API References& operator=(const References&) noexcept;
    HKSR_API RootResult Retain(void* object) const;
    // GCHandleType.Weak: observation, not ownership/resurrection tracking.
    HKSR_API RootResult Watch(void* object) const;
    HKSR_API ObjectResult Target(std::int32_t handle) const;
    HKSR_API CallResult Free(std::int32_t handle) const;
    HKSR_API ObjectResult LoadAsset(void* managed_path,void* managed_type,bool option) const;
    HKSR_API CallResult ReleaseAsset(void* object) const;
private:
    std::shared_ptr<const ReferenceMethods> methods_;
};
HKSR_API ReferenceMethods ResolveReferenceMethods(MethodResolver& resolver);
// No asset-loader dependency for code that only retains managed inputs.
HKSR_API ReferenceMethods ResolveGcMethods(MethodResolver& resolver);
}
