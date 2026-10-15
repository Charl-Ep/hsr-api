#pragma once
#include <memory>
#include "../../managed/References.hpp"
#include "../../metadata/RuntimeMetadata.hpp"
#include <vector>
#include <string>

namespace hksr::sdk_v1 {
struct AnimationEventMethods {
    Method context,caster,records,register_file,detach,dispose_record;
    std::vector<std::string> binding_errors;
    std::size_t consumer_offset=0;
    bool has_consumer=false;
    Method register_inline;
};
// Thin official calls. The caller owns component/context/path lifetimes and
// must limit mutation to registrations it created on its own entity.
// Register's result belongs to the official event list; GC retention is NOT
// registration ownership. Detach and Dispose are distinct operations.
class AnimationEvents {
public:
    HKSR_API explicit AnimationEvents(AnimationEventMethods methods);
    HKSR_API ~AnimationEvents();
    HKSR_API AnimationEvents(const AnimationEvents&) noexcept;
    HKSR_API AnimationEvents& operator=(const AnimationEvents&) noexcept;
    HKSR_API ObjectResult Context(void* component) const;
    HKSR_API ObjectResult Caster(void* task_context) const;
    HKSR_API ObjectResult Records(void* component) const;
    HKSR_API ObjectResult Consumer(void* component) const;
    HKSR_API ObjectResult Register(void* component,void* task_context,void* path) const;
    HKSR_API ObjectResult RegisterInline(void* component,void* task_context,void* events,void* behaviors,void* groups) const;
    // Option is passed through, not renamed as a claimed "dispose" flag.
    HKSR_API CallResult Detach(void* component,void* registration,bool option) const;
    HKSR_API CallResult Dispose(void* detached_registration) const;
private:
    std::shared_ptr<const AnimationEventMethods> methods_;
};
HKSR_API AnimationEventMethods ResolveAnimationEventMethods(MethodResolver& source,
    const std::vector<DeclaredField>& component_fields = {});
struct AnimationInputTypes { ObjectResult data,events; };
// Returned System.Type objects are unrooted. Version-specific component names
// stay here rather than leaking into the Framework's NPC scheduler.
HKSR_API AnimationInputTypes ResolveAnimationInputTypes(ManagedObjects& objects);
}
