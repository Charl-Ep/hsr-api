#pragma once
#include <map>
#include <string>
#include <stdexcept>
#include <cstddef>

namespace hksr::sdk_v1::detail {
// Index the provider's immutable domain inventory, not a persisted name map.
// Duplicate declarations remain ambiguous rather than choosing the first.
class MetadataTypeIndex {
    struct Entry { std::size_t index; bool ambiguous=false; };
    std::map<std::pair<std::string,std::string>,Entry> entries_;
public:
    void Add(const std::string& space,const std::string& name,std::size_t index) {
        auto result=entries_.emplace(std::make_pair(space,name),Entry{index});
        if(!result.second)result.first->second.ambiguous=true;
    }
    std::size_t Find(const char* space,const char* name) const {
        auto found=entries_.find({space,name});
        if(found==entries_.end())throw std::runtime_error("runtime type is missing from this domain snapshot");
        if(found->second.ambiguous)throw std::runtime_error("runtime type is ambiguous across images");
        return found->second.index;
    }
};
}
