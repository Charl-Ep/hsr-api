#pragma once
#include "../entity/LifecycleHooks.hpp"
#include "../managed/ManagedArrays.hpp"
#include "../managed/ManagedObjects.hpp"
#include <memory>
#include <string>

namespace hksr::sdk_v1 {
// Per-owned-model domain policy. Alternate models retain prefab/animation
// weapon visibility rather than inheriting the exploration HideWeaponList.
class HKSR_API ModelVisibilityLease {
public:
    ModelVisibilityLease(MethodResolver&,ManagedObjects&,ReferenceMethods,ManagedArrays);
    ~ModelVisibilityLease();
    ModelVisibilityLease(const ModelVisibilityLease&)=delete;
    ModelVisibilityLease& operator=(const ModelVisibilityLease&)=delete;
    bool Select(EntityToken,void* model,bool alternate);
    bool Close();
    const std::string& Error() const;
private:
    struct State;State* state_;
};
}
