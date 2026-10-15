#pragma once
#include "MethodResolver.hpp"
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
struct DeclaredMethod {
    std::string name, result;
    std::vector<std::string> parameters;
    bool is_static=false;
    Method entry;
};
struct DeclaredType {
    std::string name_space, name;
    std::vector<DeclaredMethod> methods;
};
// A runtime metadata provider supplies declarations for a single domain.
// Not a report-file loader; diagnostic JSON never becomes executable pointers.
class HKSR_API MetadataResolver final:public MethodResolver {
public:
    explicit MetadataResolver(const std::vector<DeclaredType>& declarations);
    ~MetadataResolver();
    MetadataResolver(const MetadataResolver&)=delete;
    MetadataResolver& operator=(const MetadataResolver&)=delete;
    Method Resolve(const char* name_space,const char* type,const char* name,
        const char* result,std::initializer_list<const char*> parameters,MethodKind kind=MethodKind::Instance) override;
private:
    struct Storage;
    Storage* storage_;
};
}
