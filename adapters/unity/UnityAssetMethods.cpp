#include <hksr/unity/UnityAssetAccess.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
MeshTemplateMethods ResolveMeshTemplateMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    MeshTemplateMethods m{
        b.Resolve("UnityEngine","Object","Instantiate","UnityEngine.Object",{"UnityEngine.Object"},MethodKind::Static),
        b.Resolve("UnityEngine","Mesh","get_vertexCount","System.Int32",{}),
        b.Resolve("UnityEngine","Mesh","get_blendShapeCount","System.Int32",{})
    }; m.binding_errors=b.Errors(); return m;
}
MeshMethods ResolveMeshMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    MeshMethods m{
        b.Resolve("UnityEngine","Mesh","Internal_Create","System.Void",{"UnityEngine.Mesh"},MethodKind::Static),
        b.Resolve("UnityEngine","Mesh","set_indexFormat","System.Void",{"UnityEngine.Rendering.IndexFormat"}),
        b.Resolve("UnityEngine","Mesh","SetSizedNativeArrayForChannel","System.Void",{"UnityEngine.Rendering.VertexAttribute","UnityEngine.Rendering.VertexAttributeFormat","System.Int32","System.IntPtr","System.Int32","System.Int32","System.Int32"}),
        b.Resolve("UnityEngine","Mesh","SetIndicesNativeArrayImpl","System.Void",{"System.Int32","UnityEngine.MeshTopology","UnityEngine.Rendering.IndexFormat","System.IntPtr","System.Int32","System.Int32","System.Boolean","System.Int32"}),
        b.Resolve("UnityEngine","Mesh","set_subMeshCount","System.Void",{"System.Int32"}),
        b.Resolve("UnityEngine","Mesh","RecalculateBounds","System.Void",{}),
        b.Resolve("UnityEngine","Mesh","UploadMeshData","System.Void",{"System.Boolean"}),
        b.Resolve("UnityEngine","Mesh","get_vertexCount","System.Int32",{}),
        b.Resolve("UnityEngine","Mesh","get_subMeshCount","System.Int32",{}),
        b.Resolve("UnityEngine","Mesh","HasBoneWeights","System.Boolean",{})
    }; m.binding_errors=b.Errors(); return m;
}
TextureMethods ResolveTextureMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    TextureMethods m{
        b.Resolve("UnityEngine","Texture2D",".ctor","System.Void",{"System.Int32","System.Int32","UnityEngine.TextureFormat","System.Int32","System.Boolean","System.IntPtr"}),
        b.Resolve("UnityEngine","Texture2D","LoadRawTextureDataImpl","System.Boolean",{"System.IntPtr","System.Int32"}),
        b.Resolve("UnityEngine","Texture2D","ApplyImpl","System.Void",{"System.Boolean","System.Boolean"}),
        b.Resolve("UnityEngine","Texture","set_filterMode","System.Void",{"UnityEngine.FilterMode"}),
        b.Resolve("UnityEngine","Texture","set_anisoLevel","System.Void",{"System.Int32"}),
        b.Resolve("UnityEngine","Texture","set_mipMapBias","System.Void",{"System.Single"}),
        b.Resolve("UnityEngine","Texture","set_wrapModeU","System.Void",{"UnityEngine.TextureWrapMode"}),
        b.Resolve("UnityEngine","Texture","set_wrapModeV","System.Void",{"UnityEngine.TextureWrapMode"}),
        b.Resolve("UnityEngine","Texture","set_wrapModeW","System.Void",{"UnityEngine.TextureWrapMode"}),
        b.Resolve("UnityEngine","Texture","get_dimension","UnityEngine.Rendering.TextureDimension",{})
    }; m.binding_errors=b.Errors(); return m;
}
MaterialMethods ResolveMaterialMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    MaterialMethods m{
        b.Resolve("UnityEngine","Material","CreateWithMaterial","System.Void",{"UnityEngine.Material","UnityEngine.Material"},MethodKind::Static),
        b.Resolve("UnityEngine","Shader","PropertyToID","System.Int32",{"System.String"},MethodKind::Static),
        b.Resolve("UnityEngine","Material","HasProperty","System.Boolean",{"System.Int32"}),
        b.Resolve("UnityEngine","Material","SetTexture","System.Void",{"System.Int32","UnityEngine.Texture"}),
        b.Resolve("UnityEngine","Material","SetTextureScale","System.Void",{"System.Int32","UnityEngine.Vector2"}),
        b.Resolve("UnityEngine","Material","SetTextureOffset","System.Void",{"System.Int32","UnityEngine.Vector2"})
    }; m.binding_errors=b.Errors(); return m;
}
ShaderMethods ResolveShaderMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    ShaderMethods m{
        b.Resolve("UnityEngine","Material","get_shader","UnityEngine.Shader",{}),
        b.Resolve("UnityEngine","Shader","GetPropertyCount","System.Int32",{}),
        b.Resolve("UnityEngine","Shader","GetPropertyNameId","System.Int32",{"System.Int32"}),
        b.Resolve("UnityEngine","Shader","GetPropertyType","UnityEngine.Rendering.ShaderPropertyType",{"System.Int32"}),
        b.Resolve("UnityEngine","Shader","GetPropertyTextureDimension","UnityEngine.Rendering.TextureDimension",{"System.Int32"})
    }; m.binding_errors=b.Errors(); return m;
}
}
