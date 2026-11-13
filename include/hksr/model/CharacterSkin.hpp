#pragma once
#include "../Export.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hksr::api {
namespace detail {
class CharacterSkinFactory;
}
struct CharacterSkinRow {
    std::uint32_t skin_id = 0, avatar_id = 0;
    std::string name;
};
struct CharacterSkinCatalog {
    std::vector<CharacterSkinRow> rows;
    std::vector<std::string> errors;
    bool complete = false;
};
struct CharacterSkinStatus {
    bool prepared = false, closed = false;
    std::uint32_t skin_id = 0, avatar_id = 0;
    std::string adventure_model, battle_model, adventure_config, battle_config, audio_tag;
    std::vector<std::string> adventure_event_files, battle_event_files, errors;
};
// Owner-thread official skin configuration and resource selection. No actor,
// account equipment or scene is created. Context retains a shared preparation
// through all bound actors; Close is explicit and must follow their retirement.
class CharacterSkin {
  public:
    HKSR_API ~CharacterSkin();
    CharacterSkin(const CharacterSkin&) = delete;
    CharacterSkin& operator=(const CharacterSkin&) = delete;
    HKSR_API const CharacterSkinStatus& Status() const;
    // nullopt is an official suppression result; failures throw.
    HKSR_API std::optional<std::string> MapResource(const std::string&) const;
    // TriggerSoundInAnim's candidate selection. Not a generic audio-name map;
    // callers of other sound APIs keep their own event-name contract.
    HKSR_API std::string SelectSound(const std::string&) const;
    HKSR_API bool Close();

  private:
    friend class detail::CharacterSkinFactory;
    CharacterSkin();
    struct State;
    std::unique_ptr<State> state_;
};
}
