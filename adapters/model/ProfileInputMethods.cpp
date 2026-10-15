#include <hksr/model/ProfileInputs.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
ObjectResult ProfileInputs::AllocateOperation(ManagedObjects& objects) const {
    return objects.AllocateObject("","HLCPCNFMECN");
}
ProfileInputMethods ResolveProfileInputMethods(MethodResolver& resolver) {
    ProfileInputMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result) {
        try { return resolver.Resolve(ns,type,name,result,{}); }
        catch(const std::exception& e) { out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what()); return Method{}; }
    };
    out.model_path=get("RPG.GameCore","CharacterModelComponent","get_CharAssetPath","System.String");
    out.entity_config=get("RPG.GameCore","CharacterModelComponent","get_ConfigEntityPath","System.String");
    out.operation_constructor=get("","HLCPCNFMECN",".ctor","System.Void");
    out.animator_initialized=get("UnityEngine","Animator","get_isInitialized","System.Boolean");
    return out;
}
ProfileEventFields ResolveProfileEventFields(const std::vector<DeclaredField>& fields) {
    ProfileEventFields out;
    auto get=[&](const char* name,std::size_t& offset,bool& available) {
        unsigned matches=0;
        for(const auto& field:fields) if(field.name==name && field.type=="System.String[]" &&
            field.flags_observed && !(field.flags&0x10)) { ++matches; offset=field.offset; }
        available=matches==1;
        if(!available) out.binding_errors.push_back(std::string(name)+": missing/ambiguous current instance String[] field");
    };
    get("KGGGDHNMDMK",out.paths,out.has_paths);
    get("NBNJJDIMDKE",out.layers,out.has_layers);
    return out;
}
}
