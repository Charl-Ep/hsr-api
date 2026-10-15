#include <hksr/unity/SceneObjects.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
SceneObjectMethods ResolveSceneObjectMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    SceneObjectMethods m{
        b.Resolve("UnityEngine","GameObject","get_transform","UnityEngine.Transform",{}),
        b.Resolve("UnityEngine","GameObject","GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"}),
        b.Resolve("UnityEngine","Object","get_name","System.String",{}),
        // Exact current-domain resolution; unlike the three Unity declarations,
        // this new binding has not yet been observed in the retained game dump.
        // Do not substitute raw String offsets when it is unavailable.
        b.Resolve("System","String","Equals","System.Boolean",{"System.String","System.String"},MethodKind::Static)
    }; m.binding_errors=b.Errors(); return m;
}
}
