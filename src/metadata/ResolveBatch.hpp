#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace hksr::sdk_v1::detail {
// Resolve every independent binding before reporting a group failure. A missing
// first method must not hide the remaining contracts from update diagnostics.
class ResolveBatch final:public MethodResolver {
public:
    explicit ResolveBatch(MethodResolver& source):source_(source){}
    Method Resolve(const char* space,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> parameters,MethodKind kind=MethodKind::Instance) override {
        try{return source_.Resolve(space,type,name,result,parameters,kind);}
        catch(const std::exception& error){
            std::string identity=std::string(space)+"."+type+"::"+name+"(";
            bool first=true;for(auto parameter:parameters){if(!first)identity+=", ";identity+=parameter;first=false;}
            errors_.push_back(identity+") -> "+result+": "+error.what());return {};
        }
    }
    void RequireComplete(const char* group) const {
        if(errors_.empty())return;
        std::string message=std::string(group)+" bindings unavailable:";
        for(const auto& error:errors_)message+="\n"+error;
        throw std::runtime_error(message);
    }
    const std::vector<std::string>& Errors() const { return errors_; }
private:
    MethodResolver& source_;
    std::vector<std::string> errors_;
};
}
