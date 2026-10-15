#include <hksr/unity/ComponentConfig.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
ComponentConfigMethods ResolveComponentConfigMethods(MethodResolver& source) {
    detail::ResolveBatch b(source); ComponentConfigMethods out;
    out.serialize=b.Resolve("UnityEngine","JsonUtility","ToJson","System.String",{"System.Object","System.Boolean"},MethodKind::Static);
    out.overwrite=b.Resolve("UnityEngine","JsonUtility","FromJsonOverwrite","System.Void",{"System.String","System.Object"},MethodKind::Static);
    out.add=b.Resolve("UnityEngine","GameObject","Internal_AddComponentWithType","UnityEngine.Component",{"System.Type"});
    out.type=b.Resolve("System","Object","GetType","System.Type",{});
    out.instance_id=b.Resolve("UnityEngine","Object","GetInstanceID","System.Int32",{});
    out.parent=b.Resolve("UnityEngine","Transform","get_parent","UnityEngine.Transform",{});
    out.transform=b.Resolve("UnityEngine","Component","get_transform","UnityEngine.Transform",{});
    out.string_length=b.Resolve("System","String","get_Length","System.Int32",{});
    out.string_character=b.Resolve("System","String","get_Chars","System.Char",{"System.Int32"});
    out.binding_errors=b.Errors(); return out;
}
}
