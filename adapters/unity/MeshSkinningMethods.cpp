#include <hksr/unity/MeshSkinning.hpp>
#include "../../src/unity/MeshWriterLocator.hpp"
#include <hksr/native/LoadedModule.hpp>
#include "../runtime/RuntimeProfile.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
namespace hksr::sdk_v1 {
namespace {
MeshSkinningMethods ResolveSkinning(MethodResolver& source,const detail::RuntimeProfile& profile) {
    MeshSkinningMethods out;
    out.cached_pointer=source.Resolve("UnityEngine","Object","GetCachedPtr","System.IntPtr",{});
    // Also check the paired GameAssembly identity, not Unity alone.
    detail::GameModule(profile);
    const auto module=detail::UnityModule(profile);
    std::vector<std::vector<unsigned char>> storage;
    std::vector<detail::ExecutableBytes> sections;
    for(const auto& s:module.sections) {
        if(!(s.Characteristics&IMAGE_SCN_MEM_EXECUTE) || !s.Misc.VirtualSize) continue;
        storage.emplace_back(s.Misc.VirtualSize);
        auto& bytes=storage.back();
        if(!native::CopyMemoryChecked(bytes.data(),module.base+s.VirtualAddress,bytes.size()))
            throw std::runtime_error("mesh writer executable section unreadable");
        sections.push_back({bytes.data(),bytes.size(),s.VirtualAddress});
    }
    const auto located=detail::LocateMeshWriters(sections);
    out.weights=module.base+located.weights; out.bindposes=module.base+located.bindposes;
    return out;
}
}
MeshSkinningMethods ResolveMeshSkinning(RuntimeVersion version,MethodResolver& source) {return ResolveSkinning(source,detail::RuntimeProfileFor(version));}
}
