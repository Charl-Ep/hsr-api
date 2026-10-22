#pragma once
#include <hksr/presentation/audio/OwnedSoundRequest.hpp>

namespace hksr::sdk_v1::detail {
// On-demand AudioManager binding. The cache retains reflected field metadata,
// not a global service instance across world/service replacement.
class SoundManager {
public:
    SoundManager(RuntimeMetadata&,ReferenceMethods);
    SoundRequestServices Services() const;
    ObjectResult Manager();
    bool Close();
private:
    RuntimeMetadata& metadata_;
    SoundRequestServices services_;
    ManagedRoots roots_;
    std::vector<ManagedRootId> held_;
    void* field_=nullptr;
    Method get_;
    bool closed_=false;
    void* Keep(ObjectResult);
};
}
