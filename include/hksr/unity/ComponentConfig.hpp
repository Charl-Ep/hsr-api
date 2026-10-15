#pragma once
#include <memory>
#include "../metadata/MethodResolver.hpp"
#include "../managed/References.hpp"
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
struct ComponentConfigMethods {
    Method serialize,overwrite,add,type,instance_id,parent,transform;
    Method string_length,string_character;
    std::vector<std::string> binding_errors;
};
struct TextResult { CallResult read; std::string utf8,error; };
// Official component serialization and hierarchy identity. This does not copy
// components or choose ownership. Call Add only on the caller's inactive GO;
// it can change the graph even when no component result is returned.
class ComponentConfig {
public:
    HKSR_API explicit ComponentConfig(ComponentConfigMethods methods);
    HKSR_API ~ComponentConfig();
    HKSR_API ComponentConfig(const ComponentConfig&) noexcept;
    HKSR_API ComponentConfig& operator=(const ComponentConfig&) noexcept;
    HKSR_API ObjectResult Serialize(void* component) const;
    HKSR_API CallResult Overwrite(void* managed_json,void* component) const;
    HKSR_API ObjectResult Add(void* game_object,void* managed_type) const;
    HKSR_API ObjectResult Type(void* component) const;
    HKSR_API Observed<std::int32_t> InstanceId(void* unity_object) const;
    HKSR_API ObjectResult Parent(void* transform) const;
    HKSR_API ObjectResult Transform(void* component) const;
    // Uses String accessors, not a presumed object layout or ASCII limit. This
    // is a one-time preparation path, not a per-frame text/mesh upload API.
    HKSR_API TextResult ReadText(void* rooted_string) const;
private: std::shared_ptr<const ComponentConfigMethods> methods_;
};
HKSR_API ComponentConfigMethods ResolveComponentConfigMethods(MethodResolver& source);
}
