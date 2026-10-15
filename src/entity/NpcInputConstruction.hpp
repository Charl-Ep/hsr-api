#pragma once
#include <hksr/entity/MapNpcInputs.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <functional>
#include <vector>

namespace hksr::sdk_v1::detail {
// Internal seam: production supplies the resolved runtime types, tests supply
// real local allocators. No probe report or user-authored pointer input.
struct NpcInputOperations {
    std::function<ObjectResult()> allocate_level, allocate_definition;
    std::function<ObjectResult(const char*)> new_string;
    Method get_row, level_ctor, definition_ctor;
    std::size_t npc_id_offset = 0;
};
class NpcInputConstruction {
public:
    explicit NpcInputConstruction(ReferenceMethods methods);
    void Prepare(const NpcInputOperations& ops, std::uint32_t id, const char* name, Position3 position, Rotation4 rotation);
    NpcInputStatus Status() const { return status_; }
    void* Definition();
    void* Row();
    bool Release();
private:
    ManagedRootId Keep(ObjectResult result);
    void* Target(ManagedRootId id);
    void Require(CallResult result, const char* operation);
    ManagedRoots roots_;
    std::vector<ManagedRootId> handles_;
    ManagedRootId definition_ = 0, row_ = 0;
    NpcInputStatus status_;
};
}
