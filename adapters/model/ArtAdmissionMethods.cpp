#include <hksr/model/ArtAdmission.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
ArtAdmissionMethods ResolveArtAdmissionMethods(MethodResolver& source,const std::vector<DeclaredField>& fields) {
    detail::ResolveBatch b(source); ArtAdmissionMethods out;
    out.supply=b.Resolve("RPG.GameCore","CharacterModelComponent","LoadSubUnityGO","System.Void",
        {"System.String","System.String","System.Boolean","System.Action","UnityEngine.GameObject"});
    out.unload=b.Resolve("RPG.GameCore","CharacterModelComponent","OnArtNodeUnload","System.Void",{"System.Boolean"});
    out.hide_weapons=b.Resolve("RPG.GameCore","CharacterModelComponent","_HideWeapons","System.Void",{"System.String[]"});
    out.binding_errors=b.Errors();
    unsigned count=0;
    for(const auto& field:fields) if(field.owner_namespace=="RPG.GameCore" && field.owner_name=="CharacterModelComponent" &&
        field.name=="_ArtAssetPath" && field.type=="System.String") { ++count; out.logical_path_offset=field.offset; }
    out.has_logical_path=count==1;
    if(!out.has_logical_path) out.binding_errors.push_back("CharacterModelComponent._ArtAssetPath missing or ambiguous");
    return out;
}
VisualObjectMethods ResolveVisualObjectMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    VisualObjectMethods out{
        b.Resolve("UnityEngine","GameObject",".ctor","System.Void",{}),
        b.Resolve("UnityEngine","GameObject","SetActive","System.Void",{"System.Boolean"}),
        b.Resolve("UnityEngine","Object","Instantiate","UnityEngine.Object",{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},MethodKind::Static),
        b.Resolve("UnityEngine","Component","get_gameObject","UnityEngine.GameObject",{}),
        b.Resolve("UnityEngine","Transform","get_childCount","System.Int32",{})
    }; out.binding_errors=b.Errors(); return out;
}
}
