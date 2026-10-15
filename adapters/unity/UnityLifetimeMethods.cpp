#include <hksr/unity/OwnedUnityObjects.hpp>
#include "../../src/metadata/ResolveBatch.hpp"

namespace hksr::sdk_v1 {
UnityLifetimeMethods ResolveUnityLifetimeMethods(MethodResolver& source) {
    detail::ResolveBatch batch(source);
    UnityLifetimeMethods result;
    result.alive = batch.Resolve("UnityEngine", "Object", "IsNativeObjectAlive", "System.Boolean", {"UnityEngine.Object"}, MethodKind::Static);
    result.destroy = batch.Resolve("UnityEngine", "Object", "Destroy", "System.Void", {"UnityEngine.Object"}, MethodKind::Static);
    batch.RequireComplete("owned Unity object lifetime");
    return result;
}
}
