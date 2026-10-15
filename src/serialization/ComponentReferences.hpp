#pragma once
#include <map>
#include <set>
#include <string>

namespace hksr::sdk_v1::detail {
// Serialized Unity component data is an opaque document, not runtime state.
// Only local object references are interpreted; other authored fields survive.
std::set<int> ComponentReferences(const std::string& document);
std::string RemapComponentReferences(const std::string& document,const std::map<int,int>& mapping);
}
