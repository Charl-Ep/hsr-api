#include <hksr/unity/RendererAccess.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
RendererMethods ResolveRendererMethods(MethodResolver& resolver) {
    detail::ResolveBatch batch(resolver);
    RendererMethods methods{
        batch.Resolve("UnityEngine", "SkinnedMeshRenderer", "get_sharedMesh", "UnityEngine.Mesh", {}),
        batch.Resolve("UnityEngine", "SkinnedMeshRenderer", "set_sharedMesh", "System.Void", {"UnityEngine.Mesh"}),
        batch.Resolve("UnityEngine", "Renderer", "get_sharedMaterials", "UnityEngine.Material[]", {}),
        batch.Resolve("UnityEngine", "Renderer", "set_sharedMaterials", "System.Void", {"UnityEngine.Material[]"})
    };
    batch.RequireComplete("renderer bindings");
    // Bone overrides are optional; consumers which do not change bones do
    // not depend on this pair. Prepare checks it before any bone write.
    methods.get_bones=batch.Resolve("UnityEngine","SkinnedMeshRenderer","get_bones","UnityEngine.Transform[]",{});
    methods.set_bones=batch.Resolve("UnityEngine","SkinnedMeshRenderer","set_bones","System.Void",{"UnityEngine.Transform[]"});
    return methods;
}
}
