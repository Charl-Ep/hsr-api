#include "NpcInputBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
NpcInputOperations ResolveNpcInputOperations(RuntimeMetadata& metadata) {
    const auto factory = ResolveMapNpcFactory(metadata);
    const auto definition_type = metadata.ParameterType(factory, 1);
    const auto ctor = metadata.Resolve(definition_type, ".ctor", "System.Void",
                                       {"FDKKDPIHFKL", "System.UInt32", "System.UInt32", "System.UInt32",
                                        "System.UInt32", "System.String", "UnityEngine.Vector3",
                                        "UnityEngine.Quaternion", "RPG.GameCore.LevelNPCInfo", "EGOBMOAGCFL"});
    const auto level_type = metadata.ParameterType(ctor, 8);
    NpcInputOperations ops;
    ops.definition_ctor = ctor;
    ops.level_ctor = metadata.Resolve(level_type, ".ctor", "System.Void", {});
    ops.get_row = metadata.Resolve("RPG.GameCore", "NPCDataExcelTable", "GetData", "RPG.GameCore.NPCDataRow",
                                   {"System.UInt32"}, MethodKind::Static);
    unsigned matches = 0;
    for (const auto& field : metadata.ReadFields(level_type)) {
        if (field.name == "NPCID" && field.type == "System.UInt32" && field.offset >= 16) {
            ops.npc_id_offset = field.offset;
            ++matches;
        }
    }
    if (matches != 1)
        throw std::runtime_error("private LevelNPCInfo.NPCID declaration is missing or ambiguous");
    ops.allocate_level = [&metadata, level_type] { return metadata.AllocateObject(level_type); };
    ops.allocate_definition = [&metadata, definition_type] { return metadata.AllocateObject(definition_type); };
    ops.new_string = [&metadata](const char* text) { return metadata.NewStringUtf8(text); };
    return ops;
}
}
