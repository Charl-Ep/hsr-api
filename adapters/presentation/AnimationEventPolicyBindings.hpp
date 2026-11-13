#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/unity/ComponentConfig.hpp>
namespace hksr::api::detail {
struct EventPerformer {
    bool self = false;
    std::string alias;
};
Method ResolveEventPolicyCaster(MethodResolver&);
EventPerformer ReadEventPerformer(ObjectMetadata&, const ComponentConfig&, void*, const std::vector<DeclaredField>&,
                                  const std::string&);
}
