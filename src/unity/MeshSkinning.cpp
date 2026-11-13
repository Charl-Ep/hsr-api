#include <hksr/unity/MeshSkinning.hpp>
#include <hksr/native/Invocation.hpp>
#include "../../adapters/unity/MeshWriterCalls.hpp"
namespace hksr::api {
MeshSkinning::MeshSkinning(MeshSkinningMethods methods)
    : methods_(std::make_shared<const MeshSkinningMethods>(std::move(methods))) {}
MeshSkinning::~MeshSkinning() = default;
MeshSkinning::MeshSkinning(const MeshSkinning&) noexcept = default;
MeshSkinning& MeshSkinning::operator=(const MeshSkinning&) noexcept = default;

MeshSkinningResult MeshSkinning::Write(void* mesh, const FixedBoneWeights* weights, std::int32_t vertices,
                                       const BindposeMatrix* matrices, std::int32_t bones) const {
    MeshSkinningResult out;
    if (!weights || !matrices || vertices <= 0 || bones <= 0)
        return out;
    out.native_mesh.call = native::Call(methods_->cached_pointer, &out.native_mesh.object, mesh);
    if (!out.native_mesh.call.returned || !out.native_mesh.object)
        return out;
    out.weights = detail::WriteWeights(methods_->weights, out.native_mesh.object, weights, vertices);
    if (out.weights.read.returned && out.weights.value)
        out.bindposes = detail::WriteBindposes(methods_->bindposes, out.native_mesh.object, matrices, bones);
    return out;
}
}
