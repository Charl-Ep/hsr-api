#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../managed/ManagedRoots.hpp"
#include "../../world/WorldPlacement.hpp"
#include <optional>
#include <functional>

namespace hksr::sdk_v1 {
struct EffectInputResult {CallResult target,completion;};
// Entity-relative visual inputs. Path mapping uses the explicit caster. This
// is not a TriggerEffect interpreter: no target evaluator, mutex lookup,
// loading hide, Timeline attachment or post-load subobject modifications.
struct EntityEffectOptions {
    std::string unique_name;
    Position3 position_offset{},rotation_offset{};
    std::optional<Position3> scale;
    bool attach_to_target=false,fade_in=true,simulate_immediately=false;
    std::string attach_point; // initial named attachment, independent of continued parenting
};
// Fixed world pose with no target entity or attachment. Caster remains the
// resource mapping and ownership source; the effect may animate from this pose.
struct SpatialEffectOptions {
    Position3 position{};
    Rotation4 rotation{};
    std::string unique_name;
    std::optional<Position3> scale;
    bool fade_in=true,simulate_immediately=false;
};
using ConfigureEntityEffect=std::function<EffectInputResult(void*,void*,void*,void*,const EntityEffectOptions&)>;
using ConfigureSpatialEffect=std::function<EffectInputResult(void*,void*,void*,const SpatialEffectOptions&)>;
// Engine-thread configuration of fresh unsubmitted requests. Managed references
// use FieldInfo.SetValue. Entity options write only qualified blittable fields
// at their current declared offsets and use the official attachment setter.
class HKSR_API EffectInputs {
public:
    EffectInputs(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods);
    ~EffectInputs(); // no engine calls; explicit Close required
    EffectInputs(const EffectInputs&)=delete;
    EffectInputs& operator=(const EffectInputs&)=delete;
    ObjectResult Manager(); // unrooted result, read current global each time
    EffectInputResult Configure(void* retained_parameters,void* retained_target,void* retained_completion);
    EffectInputResult ConfigureEntity(void* retained_parameters,void* retained_caster,
        void* retained_target,void* retained_completion,const EntityEffectOptions&);
    EffectInputResult ConfigureSpatial(void* retained_parameters,void* retained_caster,
        void* retained_completion,const SpatialEffectOptions&);
    bool Close();
private:
    EffectInputResult ConfigureImpl(void*,void*,void*,void*,const EntityEffectOptions&,const SpatialEffectOptions*);
    struct State;State* state_;
};
}
