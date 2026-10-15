#include "NpcInputConstruction.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
NpcInputConstruction::NpcInputConstruction(ReferenceMethods methods) : roots_(methods) { handles_.reserve(4); }
void NpcInputConstruction::Require(CallResult result, const char* operation) {
    status_.last_call = result;
    if (!result.returned) throw std::runtime_error(operation);
}
ManagedRootId NpcInputConstruction::Keep(ObjectResult result) {
    Require(result.call, "input object creation/load failed");
    if (!result.object) throw std::runtime_error("input object is null");
    const auto id = roots_.Keep(result.object); handles_.push_back(id);
    if (!roots_.Status(id).ready) throw std::runtime_error("input GC root failed");
    return id;
}
void* NpcInputConstruction::Target(ManagedRootId id) {
    const auto result = roots_.Target(id);
    Require(result.call, "input GC target read failed");
    if (!result.object) throw std::runtime_error("input GC target is null");
    return result.object;
}
void NpcInputConstruction::Prepare(const NpcInputOperations& ops, std::uint32_t id, const char* name, Position3 position, Rotation4 rotation) {
    if (status_.phase != NpcInputPhase::NotStarted) throw std::logic_error("NPC inputs are single-use");
    // No side effects until the actual constructor/cleanup dependencies exist.
    if (!name || !ops.allocate_level || !ops.allocate_definition || !ops.new_string ||
        !ops.get_row.code || !ops.level_ctor.code || !ops.definition_ctor.code || ops.npc_id_offset < 16)
        throw std::invalid_argument("incomplete private NPC construction contract");
    try {
        status_.phase = NpcInputPhase::Row;
        ObjectResult row; row.call = native::Call(ops.get_row, &row.object, id); row_ = Keep(row);
        status_.phase = NpcInputPhase::Name;
        const auto label = Keep(ops.new_string(name));
        status_.phase = NpcInputPhase::LevelAllocation;
        const auto level = Keep(ops.allocate_level());
        status_.phase = NpcInputPhase::LevelDefaults;
        Require(native::CallVoid(ops.level_ctor, Target(level)), "LevelNPCInfo constructor failed");
        status_.phase = NpcInputPhase::NpcId;
        // Only a primitive field on our newly allocated, rooted data object.
        // No managed-reference write and no mutation of NPCDataExcelTable row.
        auto* field = static_cast<unsigned char*>(Target(level)) + ops.npc_id_offset;
        if (!native::CopyMemoryChecked(field, &id, sizeof(id))) throw std::runtime_error("private NPCID write failed");
        status_.phase = NpcInputPhase::DefinitionAllocation;
        definition_ = Keep(ops.allocate_definition());
        status_.phase = NpcInputPhase::DefinitionConstructor;
        // Verified x64 call uses indirect Vector3/Quaternion value arguments.
        Require(native::CallVoid(ops.definition_ctor, Target(definition_), static_cast<void*>(nullptr),
            std::uint32_t(0), std::uint32_t(0), std::uint32_t(0), id,
            Target(label), &position, &rotation, Target(level), static_cast<void*>(nullptr)), "MapNpcDef constructor failed");
        status_.phase = NpcInputPhase::Ready; status_.ready = true;
    } catch (const std::exception& error) { status_.error = error.what(); }
}
void* NpcInputConstruction::Definition() { return status_.ready ? Target(definition_) : nullptr; }
void* NpcInputConstruction::Row() { return status_.ready ? Target(row_) : nullptr; }
bool NpcInputConstruction::Release() {
    status_.ready = false;
    bool complete = true;
    for (auto& id : handles_) {
        if (!id) continue;
        if (roots_.Release(id).complete) { roots_.Forget(id); id = 0; }
        else complete = false;
    }
    status_.roots_released = complete; return complete;
}
}
