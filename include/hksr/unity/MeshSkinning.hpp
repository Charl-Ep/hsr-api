#pragma once
#include <memory>
#include <hksr/runtime/GameVersion.hpp>
#include "../managed/References.hpp"
#include <array>
#include <cstdint>

namespace hksr::sdk_v1 {
struct FixedBoneWeights { std::array<float,4> weights; std::array<std::int32_t,4> indices; };
using BindposeMatrix=std::array<float,16>;
struct MeshSkinningMethods {
    Method cached_pointer;
    void* weights=nullptr;
    void* bindposes=nullptr;
};
struct MeshSkinningResult {
    ObjectResult native_mesh;
    Observed<bool> weights;
    CallResult bindposes;
    bool Returned() const { return native_mesh.call.returned && native_mesh.object && weights.read.returned && weights.value && bindposes.returned; }
};
// Caller holds a private initialized Mesh with its vertex count already set.
// Fixed four-weight arrays and matrices are synchronous borrowed input. Does
// not bind bones, assign renderers, adapt topology or declare visual equivalence.
class MeshSkinning {
public:
    HKSR_API explicit MeshSkinning(MeshSkinningMethods methods);
    HKSR_API ~MeshSkinning();
    HKSR_API MeshSkinning(const MeshSkinning&) noexcept;
    HKSR_API MeshSkinning& operator=(const MeshSkinning&) noexcept;
    HKSR_API MeshSkinningResult Write(void* rooted_mesh,const FixedBoneWeights* weights,std::int32_t vertices,
        const BindposeMatrix* bindposes,std::int32_t bones) const;
private: std::shared_ptr<const MeshSkinningMethods> methods_;
};
// Resolve once per live engine/module lifetime, not once per frame or mesh.
// Locks the retained version and derives targets from executable relations.
HKSR_API MeshSkinningMethods ResolveMeshSkinning(RuntimeVersion version,MethodResolver& source);
}
