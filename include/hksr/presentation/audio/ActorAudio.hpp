#pragma once
#include <cstdint>
#include <string>

namespace hksr::api {
// Exact event names and qualified bank audio-node IDs. Bank preparation and
// logical request identities remain owned by the official AudioManager.
struct ActorAudioEvent {
    std::string event;
    std::uint32_t normal_node = 0, doubled_node = 0;
};
}
