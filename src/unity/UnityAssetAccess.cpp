#include <hksr/unity/UnityAssetAccess.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::sdk_v1 {
MeshTemplateAccess::MeshTemplateAccess(MeshTemplateMethods methods):methods_(std::make_shared<const MeshTemplateMethods>(std::move(methods))){}
MeshTemplateAccess::~MeshTemplateAccess()=default;
MeshTemplateAccess::MeshTemplateAccess(const MeshTemplateAccess&) noexcept=default;
MeshTemplateAccess& MeshTemplateAccess::operator=(const MeshTemplateAccess&) noexcept=default;

MeshAccess::MeshAccess(MeshMethods methods):methods_(std::make_shared<const MeshMethods>(std::move(methods))){}
MeshAccess::~MeshAccess()=default;
MeshAccess::MeshAccess(const MeshAccess&) noexcept=default;
MeshAccess& MeshAccess::operator=(const MeshAccess&) noexcept=default;

TextureAccess::TextureAccess(TextureMethods methods):methods_(std::make_shared<const TextureMethods>(std::move(methods))){}
TextureAccess::~TextureAccess()=default;
TextureAccess::TextureAccess(const TextureAccess&) noexcept=default;
TextureAccess& TextureAccess::operator=(const TextureAccess&) noexcept=default;

MaterialAccess::MaterialAccess(MaterialMethods methods):methods_(std::make_shared<const MaterialMethods>(std::move(methods))){}
MaterialAccess::~MaterialAccess()=default;
MaterialAccess::MaterialAccess(const MaterialAccess&) noexcept=default;
MaterialAccess& MaterialAccess::operator=(const MaterialAccess&) noexcept=default;

ShaderAccess::ShaderAccess(ShaderMethods methods):methods_(std::make_shared<const ShaderMethods>(std::move(methods))){}
ShaderAccess::~ShaderAccess()=default;
ShaderAccess::ShaderAccess(const ShaderAccess&) noexcept=default;
ShaderAccess& ShaderAccess::operator=(const ShaderAccess&) noexcept=default;

ObjectResult MeshTemplateAccess::Clone(void* mesh) const {
    ObjectResult out; out.call=native::Call(methods_->clone,&out.object,mesh); return out;
}
Observed<std::int32_t> MeshTemplateAccess::VertexCount(void* mesh) const {
    Observed<std::int32_t> out; out.read=native::Call(methods_->vertices,&out.value,mesh); return out;
}
Observed<std::int32_t> MeshTemplateAccess::ShapeCount(void* mesh) const {
    Observed<std::int32_t> out; out.read=native::Call(methods_->shapes,&out.value,mesh); return out;
}
CallResult MeshAccess::Initialize(void* mesh) const { return native::CallVoid(methods_->create, mesh); }
CallResult MeshAccess::IndexFormat(void* mesh, MeshIndex format) const { return native::CallVoid(methods_->index_format, mesh, format); }
CallResult MeshAccess::Channel(void* mesh, MeshAttribute attr, MeshElement format, std::int32_t dim, const void* data, std::int32_t count) const {
    return native::CallVoid(methods_->channel, mesh, attr, format, dim, data, count, 0, count);
}
CallResult MeshAccess::Submeshes(void* mesh, std::int32_t count) const { return native::CallVoid(methods_->submeshes, mesh, count); }
CallResult MeshAccess::Triangles(void* mesh, std::int32_t submesh, MeshIndex format, const void* data, std::int32_t first, std::int32_t count) const {
    return native::CallVoid(methods_->indices, mesh, submesh, 0, format, data, first, count, false, 0);
}
CallResult MeshAccess::RecalculateBounds(void* mesh) const { return native::CallVoid(methods_->recalculate, mesh); }
CallResult MeshAccess::Upload(void* mesh, bool discard) const { return native::CallVoid(methods_->upload, mesh, discard); }
MeshCounts MeshAccess::Counts(void* mesh) const {
    MeshCounts r;
    r.vertices.read=native::Call(methods_->vertex_count, &r.vertices.value, mesh);
    r.submeshes.read=native::Call(methods_->submesh_count, &r.submeshes.value, mesh);
    r.has_weights.read=native::Call(methods_->has_weights, &r.has_weights.value, mesh);
    return r;
}
CallResult TextureAccess::Initialize(void* texture, TextureParameters p) const {
    return native::CallVoid(methods_->construct, texture, p.width, p.height, p.format, p.mip_count, p.linear, static_cast<void*>(nullptr));
}
Observed<bool> TextureAccess::LoadRaw(void* texture, const void* bytes, std::int32_t size) const {
    Observed<bool> r; r.read=native::Call(methods_->load, &r.value, texture, bytes, size); return r;
}
TextureSamplingResult TextureAccess::Sampling(void* texture, TextureSampling p) const {
    return {native::CallVoid(methods_->filter, texture, p.filter), native::CallVoid(methods_->aniso, texture, p.aniso),
        native::CallVoid(methods_->mip_bias, texture, p.mip_bias), native::CallVoid(methods_->wrap_u, texture, p.wrap_u),
        native::CallVoid(methods_->wrap_v, texture, p.wrap_v), native::CallVoid(methods_->wrap_w, texture, p.wrap_w)};
}
CallResult TextureAccess::Apply(void* texture, bool update, bool discard) const { return native::CallVoid(methods_->apply, texture, update, discard); }
Observed<std::int32_t> TextureAccess::Dimension(void* texture) const {
    Observed<std::int32_t> out; out.read=native::Call(methods_->dimension,&out.value,texture); return out;
}
CallResult MaterialAccess::InitializeCopy(void* material, void* source) const { return native::CallVoid(methods_->copy, material, source); }
Observed<std::int32_t> MaterialAccess::PropertyId(void* name) const {
    Observed<std::int32_t> r; r.read=native::Call(methods_->property_id, &r.value, name); return r;
}
Observed<bool> MaterialAccess::HasProperty(void* material, std::int32_t id) const {
    Observed<bool> r; r.read=native::Call(methods_->has_property, &r.value, material, id); return r;
}
CallResult MaterialAccess::Texture(void* material, std::int32_t id, void* texture) const { return native::CallVoid(methods_->set_texture, material, id, texture); }
CallResult MaterialAccess::Scale(void* material, std::int32_t id, Float2 value) const { return native::CallVoid(methods_->scale, material, id, value); }
CallResult MaterialAccess::Offset(void* material, std::int32_t id, Float2 value) const { return native::CallVoid(methods_->offset, material, id, value); }
ShaderProperties ShaderAccess::Inspect(void* material) const {
    ShaderProperties out;
    out.shader.call=native::Call(methods_->material_shader,&out.shader.object,material);
    if(!out.shader.call.returned || !out.shader.object) return out;
    out.count.read=native::Call(methods_->count,&out.count.value,out.shader.object);
    if(!out.count.read.returned || out.count.value<0) return out;
    out.complete=true;
    out.properties.reserve(static_cast<std::size_t>(out.count.value));
    for(std::int32_t i=0;i<out.count.value;++i) {
        ShaderProperty p;
        p.id.read=native::Call(methods_->id,&p.id.value,out.shader.object,i);
        p.type.read=native::Call(methods_->type,&p.type.value,out.shader.object,i);
        if(p.type.read.returned && p.type.value==4)
            p.texture_dimension.read=native::Call(methods_->texture_dimension,&p.texture_dimension.value,out.shader.object,i);
        out.complete=out.complete && p.id.read.returned && p.type.read.returned &&
            (p.type.value!=4 || p.texture_dimension.read.returned);
        out.properties.push_back(p);
    }
    return out;
}
}
