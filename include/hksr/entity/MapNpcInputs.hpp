#pragma once
#include "../metadata/RuntimeMetadata.hpp"
#include "EntityFactory.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct Position3 { float x = 0, y = 0, z = 0; };
struct Rotation4 { float x = 0, y = 0, z = 0, w = 1; };
enum class NpcInputPhase { NotStarted, Row, Name, LevelAllocation, LevelDefaults, NpcId, DefinitionAllocation, DefinitionConstructor, Ready };
struct NpcInputStatus {
    NpcInputPhase phase = NpcInputPhase::NotStarted;
    CallResult last_call;
    std::string error;
    bool ready = false;
    bool roots_released = false;
};
// Private input objects; does not create/register a scene actor. Retain this
// owner for the duration of its consumer's official creation/loading operation.
class HKSR_API MapNpcInputs {
public:
    static std::unique_ptr<MapNpcInputs> Prepare(RuntimeMetadata& metadata,
        ReferenceMethods references, std::uint32_t npc_id, const char* name,
        Position3 position, Rotation4 rotation);
    ~MapNpcInputs(); // no engine calls; explicit Release on engine thread
    MapNpcInputs(const MapNpcInputs&) = delete;
    MapNpcInputs& operator=(const MapNpcInputs&) = delete;
    NpcInputStatus Status() const;
    MapNpcDefinition* Definition();
    NpcDataRow* Row();
    bool Release();
private:
    struct State;
    explicit MapNpcInputs(State* state);
    State* state_;
};
}
