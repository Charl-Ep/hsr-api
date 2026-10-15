#include "ComponentReferences.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
namespace {
using Json=nlohmann::json;
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int ReferenceId(const Json& value) {
    Require(value.is_number_integer(),"component reference id is not an integer");
    const auto id=value.get<std::int64_t>();
    Require(id>=std::numeric_limits<int>::min() && id<=std::numeric_limits<int>::max(),"component reference id is outside Int32");
    return static_cast<int>(id);
}
void VisitReferences(Json& value,const std::function<void(Json&,int)>& visit) {
    if(value.is_object()) {
        if(value.contains("m_FileID") && value.contains("m_PathID")) {
            Require(value.at("m_PathID")==0,"LookAt input has an external asset reference");
            const auto id=ReferenceId(value.at("m_FileID")); if(id) visit(value,id);
            return;
        }
        for(auto& child:value.items()) VisitReferences(child.value(),visit);
    } else if(value.is_array()) for(auto& child:value) VisitReferences(child,visit);
}
Json Document(const std::string& text) {
    auto value=Json::parse(text);
    Require(value.is_object(),"component serialization is not an object");return value;
}
}
std::set<int> ComponentReferences(const std::string& document) {
    auto value=Document(document);std::set<int> result;
    VisitReferences(value,[&](Json&,int id){result.insert(id);});return result;
}
std::string RemapComponentReferences(const std::string& document,const std::map<int,int>& mapping) {
    auto value=Document(document);
    VisitReferences(value,[&](Json& reference,int id){
        const auto found=mapping.find(id);
        Require(found!=mapping.end(),"component reference has no private transform correspondence");
        reference["m_FileID"]=found->second;
    });
    return value.dump();
}
}
