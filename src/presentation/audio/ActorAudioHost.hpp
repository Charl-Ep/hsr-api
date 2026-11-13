#pragma once
#include <hksr/presentation/audio/ActorAudio.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/presentation/audio/VoiceCue.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace hksr::api {
class RuntimeMetadata;
struct ManagedObjects;
class OwnedSoundRequest;
namespace detail {
// Owns one client's actor audio bindings, requests, callback route and GC roots.
// It never owns or writes the world's global audio state.
class ActorAudioHost {
  public:
    ActorAudioHost(ManagedObjects&, ReferenceMethods, EntityAccess, ModelAccess, EntityOwner, ManagedArrays,
                   ComponentConfig);
    ~ActorAudioHost();
    void Prepare(RuntimeMetadata&, std::function<void(const std::string&)>);
    std::unique_ptr<OwnedSoundRequest> Start(EntityToken, GameWorld*, const std::string&) const;
    void Set(EntityToken, GameWorld*, std::vector<ActorAudioEvent>, unsigned);
    void SetRate(EntityToken, GameWorld*, unsigned);
    void Clear(EntityToken, GameWorld*);
    std::string ReadVoiceEvent(EntityToken, GameWorld*, std::uint32_t, VoiceCue) const;
    std::optional<VoiceCue> ReadReadyVoiceCue(std::int32_t) const;
    bool VoiceExists(const std::string&) const;
    void Refresh(GameEntity*);
    void Activate();
    void Collect();
    void BeginClose();
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
}
