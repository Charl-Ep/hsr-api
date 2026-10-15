#include <hksr/unity/MeshSkinning.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::sdk_v1 {
MeshSkinning::MeshSkinning(MeshSkinningMethods methods):methods_(std::make_shared<const MeshSkinningMethods>(std::move(methods))){}
MeshSkinning::~MeshSkinning()=default;
MeshSkinning::MeshSkinning(const MeshSkinning&) noexcept=default;
MeshSkinning& MeshSkinning::operator=(const MeshSkinning&) noexcept=default;

static_assert(sizeof(FixedBoneWeights)==32 && sizeof(BindposeMatrix)==64);
namespace {
Observed<bool> WriteWeights(void* entry,void* mesh,const FixedBoneWeights* data,std::int32_t count) {
    Observed<bool> out;
    if(!entry) return out;
    __try {
        out.value=reinterpret_cast<bool(__fastcall*)(void*,const void*,std::int32_t)>(entry)(mesh,data,count);
        out.read={true,0};
    } __except(EXCEPTION_EXECUTE_HANDLER) { out.read={false,GetExceptionCode()}; }
    return out;
}
CallResult WriteBindposes(void* entry,void* mesh,const BindposeMatrix* data,std::int32_t count) {
    if(!entry) return {};
    __try {
        reinterpret_cast<void(__fastcall*)(void*,const void*,std::int32_t)>(entry)(mesh,data,count);
        return {true,0};
    } __except(EXCEPTION_EXECUTE_HANDLER) { return {false,GetExceptionCode()}; }
}
}
MeshSkinningResult MeshSkinning::Write(void* mesh,const FixedBoneWeights* weights,std::int32_t vertices,const BindposeMatrix* matrices,std::int32_t bones) const {
    MeshSkinningResult out;
    if(!weights || !matrices || vertices<=0 || bones<=0) return out;
    out.native_mesh.call=native::Call(methods_->cached_pointer,&out.native_mesh.object,mesh);
    if(!out.native_mesh.call.returned || !out.native_mesh.object) return out;
    out.weights=WriteWeights(methods_->weights,out.native_mesh.object,weights,vertices);
    if(out.weights.read.returned && out.weights.value)
        out.bindposes=WriteBindposes(methods_->bindposes,out.native_mesh.object,matrices,bones);
    return out;
}
}
