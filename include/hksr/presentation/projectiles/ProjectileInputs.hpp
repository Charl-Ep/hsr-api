#pragma once
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <map>

namespace hksr::sdk_v1 {
enum class ProjectileFamily { Linear=1, RandomTrace=2, HalfBoomerang=4, PrefabCurve=7 };
struct ProjectileInputStatus {
    bool prepared=false,legacy=false,closed=false;
    ProjectileFamily family=ProjectileFamily::Linear;
    // Borrowed, retained until Close. The source configuration is never edited.
    void* parameters=nullptr;
    void* hit_effect=nullptr;
    bool align_hit=false,attach_hit=false,start_from_last_hit=false,use_last_velocity=false;
    bool expects_effect=false;
    std::vector<std::string> unresolved_values;
    std::string error;
};
// Builds one private visual configuration. Expressions require an explicitly
// evaluated value keyed by field name; fixedValue is not an expression result.
// Preserves trajectory/curve fields and returns hit presentation to the owner.
class ProjectileInputs {
public:
    HKSR_API ProjectileInputs(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods);
    HKSR_API ~ProjectileInputs();
    ProjectileInputs(const ProjectileInputs&)=delete;
    ProjectileInputs& operator=(const ProjectileInputs&)=delete;
    HKSR_API bool Prepare(void* source,const std::map<std::string,float>& values={});
    HKSR_API const ProjectileInputStatus& Status() const;
    HKSR_API bool Close();
private:
    struct State;std::unique_ptr<State> state_;
};
}
