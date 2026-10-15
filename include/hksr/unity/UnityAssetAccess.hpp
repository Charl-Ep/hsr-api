#pragma once
#include <memory>
#include "../managed/References.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace hksr::sdk_v1 {
// All objects are managed references. The caller roots and owns newly allocated
// objects BEFORE initialization. A failed constructor can still have created a
// native resource: keep the original reference and close via OwnedUnityObjects.
// These calls never select a live player's resources or change a renderer.
struct MeshMethods {
    Method create, index_format, channel, indices, submeshes, recalculate, upload;
    Method vertex_count, submesh_count, has_weights;
    std::vector<std::string> binding_errors;
};
enum class MeshAttribute : std::int32_t { Position=0, Normal=1, Tangent=2, Color=3, UV0=4, UV1=5 };
enum class MeshElement : std::int32_t { Float32=0, UNorm8=2 };
enum class MeshIndex : std::int32_t { UInt16=0, UInt32=1 };
struct MeshCounts { Observed<std::int32_t> vertices, submeshes; Observed<bool> has_weights; };
struct MeshTemplateMethods { Method clone, vertices, shapes; std::vector<std::string> binding_errors; };
class MeshTemplateAccess {
public:
    HKSR_API explicit MeshTemplateAccess(MeshTemplateMethods methods);
    HKSR_API ~MeshTemplateAccess();
    HKSR_API MeshTemplateAccess(const MeshTemplateAccess&) noexcept;
    HKSR_API MeshTemplateAccess& operator=(const MeshTemplateAccess&) noexcept;
    // Only use an independently loaded Mesh, not a GameObject hierarchy. The
    // returned native clone is owned by the caller; retain it immediately.
    HKSR_API ObjectResult Clone(void* rooted_template_mesh) const;
    HKSR_API Observed<std::int32_t> VertexCount(void* mesh) const;
    HKSR_API Observed<std::int32_t> ShapeCount(void* mesh) const;
private: std::shared_ptr<const MeshTemplateMethods> methods_;
};
class MeshAccess {
public:
    HKSR_API explicit MeshAccess(MeshMethods methods);
    HKSR_API ~MeshAccess();
    HKSR_API MeshAccess(const MeshAccess&) noexcept;
    HKSR_API MeshAccess& operator=(const MeshAccess&) noexcept;
    HKSR_API CallResult Initialize(void* allocated_mesh) const;
    HKSR_API CallResult IndexFormat(void* mesh, MeshIndex format) const;
    // Synchronous native buffer upload. Count/start/length are elements, NOT
    // bytes; the caller supplies storage for the declared format/dimension.
    HKSR_API CallResult Channel(void* mesh, MeshAttribute attribute, MeshElement format,
        std::int32_t dimension, const void* data, std::int32_t count) const;
    HKSR_API CallResult Submeshes(void* mesh, std::int32_t count) const;
    HKSR_API CallResult Triangles(void* mesh, std::int32_t submesh, MeshIndex format,
        const void* data, std::int32_t first, std::int32_t count) const;
    HKSR_API CallResult RecalculateBounds(void* mesh) const;
    HKSR_API CallResult Upload(void* mesh, bool discard_cpu_copy) const;
    HKSR_API MeshCounts Counts(void* mesh) const;
private: std::shared_ptr<const MeshMethods> methods_;
};
struct TextureMethods {
    Method construct, load, apply, filter, aniso, mip_bias, wrap_u, wrap_v, wrap_w, dimension;
    std::vector<std::string> binding_errors;
};
struct TextureParameters {
    std::int32_t width=0, height=0, format=0, mip_count=0;
    bool linear=false;
};
struct TextureSampling {
    std::int32_t filter=0, aniso=0, wrap_u=0, wrap_v=0, wrap_w=0;
    float mip_bias=0;
};
struct TextureSamplingResult {
    CallResult filter, aniso, mip_bias, wrap_u, wrap_v, wrap_w;
    bool Returned() const { return filter.returned && aniso.returned && mip_bias.returned && wrap_u.returned && wrap_v.returned && wrap_w.returned; }
};
class TextureAccess {
public:
    HKSR_API explicit TextureAccess(TextureMethods methods);
    HKSR_API ~TextureAccess();
    HKSR_API TextureAccess(const TextureAccess&) noexcept;
    HKSR_API TextureAccess& operator=(const TextureAccess&) noexcept;
    HKSR_API CallResult Initialize(void* allocated_texture, TextureParameters parameters) const;
    HKSR_API Observed<bool> LoadRaw(void* texture, const void* bytes, std::int32_t size) const;
    HKSR_API TextureSamplingResult Sampling(void* texture, TextureSampling parameters) const;
    HKSR_API CallResult Apply(void* texture, bool update_mips, bool discard_cpu_copy) const;
    HKSR_API Observed<std::int32_t> Dimension(void* texture) const;
private: std::shared_ptr<const TextureMethods> methods_;
};
struct MaterialMethods { Method copy, property_id, has_property, set_texture, scale, offset; std::vector<std::string> binding_errors; };
struct Float2 { float x, y; };
class MaterialAccess {
public:
    HKSR_API explicit MaterialAccess(MaterialMethods methods);
    HKSR_API ~MaterialAccess();
    HKSR_API MaterialAccess(const MaterialAccess&) noexcept;
    HKSR_API MaterialAccess& operator=(const MaterialAccess&) noexcept;
    HKSR_API CallResult InitializeCopy(void* allocated_material, void* independent_template) const;
    HKSR_API Observed<std::int32_t> PropertyId(void* managed_name) const;
    HKSR_API Observed<bool> HasProperty(void* material, std::int32_t id) const;
    HKSR_API CallResult Texture(void* material, std::int32_t id, void* texture) const;
    HKSR_API CallResult Scale(void* material, std::int32_t id, Float2 value) const;
    HKSR_API CallResult Offset(void* material, std::int32_t id, Float2 value) const;
private: std::shared_ptr<const MaterialMethods> methods_;
};
struct ShaderMethods {
    Method material_shader, count, id, type, texture_dimension;
    std::vector<std::string> binding_errors;
};
struct ShaderProperty {
    Observed<std::int32_t> id, type, texture_dimension;
};
struct ShaderProperties {
    ObjectResult shader;
    Observed<std::int32_t> count;
    std::vector<ShaderProperty> properties;
    bool complete=false;
};
class ShaderAccess {
public:
    HKSR_API explicit ShaderAccess(ShaderMethods methods);
    HKSR_API ~ShaderAccess();
    HKSR_API ShaderAccess(const ShaderAccess&) noexcept;
    HKSR_API ShaderAccess& operator=(const ShaderAccess&) noexcept;
    // Synchronous snapshot of the actual material's shader. Non-texture
    // properties do not call the texture-only dimension getter.
    HKSR_API ShaderProperties Inspect(void* rooted_material) const;
private: std::shared_ptr<const ShaderMethods> methods_;
};
// Collect every binding result. Missing readback or texture calls do not block
// independent operations; consumers decide their actual required operations.
HKSR_API MeshMethods ResolveMeshMethods(MethodResolver& source);
HKSR_API MeshTemplateMethods ResolveMeshTemplateMethods(MethodResolver& source);
HKSR_API TextureMethods ResolveTextureMethods(MethodResolver& source);
HKSR_API MaterialMethods ResolveMaterialMethods(MethodResolver& source);
HKSR_API ShaderMethods ResolveShaderMethods(MethodResolver& source);
}
