#include <hksr/metadata/MetadataResolver.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct MetadataResolver::Storage { std::vector<DeclaredType> types; };
MetadataResolver::MetadataResolver(const std::vector<DeclaredType>& declarations):storage_(new Storage{declarations}){}
MetadataResolver::~MetadataResolver(){delete storage_;}
Method MetadataResolver::Resolve(const char* space,const char* type,const char* name,const char* result,
    std::initializer_list<const char*> parameters,MethodKind kind){
    const DeclaredType* selected=nullptr;
    for(const auto& item:storage_->types)if(item.name_space==space && item.name==type){
        if(selected)throw std::runtime_error("declared type is ambiguous");selected=&item;
    }
    if(!selected)throw std::runtime_error("declared type is missing");
    const std::vector<std::string> expected(parameters.begin(),parameters.end());
    const DeclaredMethod* found=nullptr;
    for(const auto& method:selected->methods){
        if(method.name!=name || method.result!=result || method.parameters!=expected || method.is_static!=(kind==MethodKind::Static))continue;
        if(found)throw std::runtime_error("declared method is ambiguous");found=&method;
    }
    if(!found || !found->entry.code)throw std::runtime_error("declared method is missing or unbound");
    return found->entry;
}
}
