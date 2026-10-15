#include <hksr/entity/MapNpcInputs.hpp>
#include "NpcInputConstruction.hpp"
#include <stdexcept>

namespace hksr::sdk_v1 {
struct MapNpcInputs::State {
    detail::NpcInputConstruction input;
    explicit State(ReferenceMethods methods) : input(methods) {}
};
MapNpcInputs::MapNpcInputs(State* state) : state_(state) {}
MapNpcInputs::~MapNpcInputs() { delete state_; }
std::unique_ptr<MapNpcInputs> MapNpcInputs::Prepare(RuntimeMetadata& metadata, ReferenceMethods references,
    std::uint32_t id, const char* name, Position3 position, Rotation4 rotation) {
    const auto factory = ResolveMapNpcFactory(metadata);
    const auto definition_type = metadata.ParameterType(factory, 1);
    const auto ctor = metadata.Resolve(definition_type, ".ctor", "System.Void",
        {"FDKKDPIHFKL", "System.UInt32", "System.UInt32", "System.UInt32", "System.UInt32", "System.String",
         "UnityEngine.Vector3", "UnityEngine.Quaternion", "RPG.GameCore.LevelNPCInfo", "EGOBMOAGCFL"});
    const auto level_type = metadata.ParameterType(ctor, 8);
    detail::NpcInputOperations ops;
    ops.definition_ctor = ctor;
    ops.level_ctor = metadata.Resolve(level_type, ".ctor", "System.Void", {});
    ops.get_row = metadata.Resolve("RPG.GameCore", "NPCDataExcelTable", "GetData", "RPG.GameCore.NPCDataRow", {"System.UInt32"}, MethodKind::Static);
    unsigned matches = 0;
    for (const auto& field : metadata.ReadFields(level_type)) {
        if (field.name == "NPCID" && field.type == "System.UInt32" && field.offset >= 16) {
            ops.npc_id_offset = field.offset; ++matches;
        }
    }
    if (matches != 1) throw std::runtime_error("private LevelNPCInfo.NPCID declaration is missing or ambiguous");
    ops.allocate_level = [&metadata, level_type] { return metadata.AllocateObject(level_type); };
    ops.allocate_definition = [&metadata, definition_type] { return metadata.AllocateObject(definition_type); };
    ops.new_string = [&metadata](const char* text) { return metadata.NewStringUtf8(text); };
    auto state = std::make_unique<State>(references);
    auto result = std::unique_ptr<MapNpcInputs>(new MapNpcInputs(state.get())); state.release();
    result->state_->input.Prepare(ops, id, name, position, rotation);
    return result; // retains partial failure receipts as well as successful inputs
}
NpcInputStatus MapNpcInputs::Status() const { return state_->input.Status(); }
MapNpcDefinition* MapNpcInputs::Definition() { return static_cast<MapNpcDefinition*>(state_->input.Definition()); }
NpcDataRow* MapNpcInputs::Row() { return static_cast<NpcDataRow*>(state_->input.Row()); }
bool MapNpcInputs::Release() { return state_->input.Release(); }
}
