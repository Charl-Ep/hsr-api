#include <hksr/managed/References.hpp>
#include "../../src/metadata/ResolveBatch.hpp"

namespace hksr::sdk_v1 {
ReferenceMethods ResolveGcMethods(MethodResolver& resolver){
    detail::ResolveBatch batch(resolver);
    ReferenceMethods result{};
    result.retain=batch.Resolve("System.Runtime.InteropServices","GCHandle","GetTargetHandle","System.Int32",
        {"System.Object","System.Int32","System.Runtime.InteropServices.GCHandleType"},MethodKind::Static);
    result.get=batch.Resolve("System.Runtime.InteropServices","GCHandle","GetTarget","System.Object",{"System.Int32"},MethodKind::Static);
    result.free=batch.Resolve("System.Runtime.InteropServices","GCHandle","FreeHandle","System.Void",{"System.Int32"},MethodKind::Static);
    batch.RequireComplete("managed references");return result;
}
ReferenceMethods ResolveReferenceMethods(MethodResolver& resolver){
    detail::ResolveBatch batch(resolver);
    ReferenceMethods result{
        batch.Resolve("System.Runtime.InteropServices","GCHandle","GetTargetHandle","System.Int32",
            {"System.Object","System.Int32","System.Runtime.InteropServices.GCHandleType"},MethodKind::Static),
        batch.Resolve("System.Runtime.InteropServices","GCHandle","GetTarget","System.Object",{"System.Int32"},MethodKind::Static),
        batch.Resolve("System.Runtime.InteropServices","GCHandle","FreeHandle","System.Void",{"System.Int32"},MethodKind::Static),
        batch.Resolve("RPG.Client","CachedAssetLoader","SyncLoadAsset","UnityEngine.Object",{"System.String","System.Type","System.Boolean"},MethodKind::Static),
        batch.Resolve("RPG.Client","CachedAssetLoader","ReleaseAsset","System.Void",{"UnityEngine.Object"},MethodKind::Static)
    };
    batch.RequireComplete("GC and resource lifetime");return result;
}
}
