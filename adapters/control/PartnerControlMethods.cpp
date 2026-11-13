#include <hksr/control/PartnerControlSession.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
#include <stdexcept>

namespace hksr::api {
PartnerControlMethods ResolvePartnerControl(MethodResolver& m, const std::vector<DeclaredField>& components,
                                            const std::vector<DeclaredField>& attachment,
                                            const std::vector<DeclaredField>& visibility) {
    auto offset = [](const auto& rows, const char* owner, const char* name, const char* type) {
        const DeclaredField* found = nullptr;
        for (const auto& row : rows)
            if (row.owner_name == owner && row.name == name) {
                if (found || row.type != type || !row.flags_observed || (row.flags & 0x10))
                    throw std::invalid_argument(std::string(owner) + "." + name + " declaration differs");
                found = &row;
            }
        if (!found)
            throw std::invalid_argument(std::string(owner) + "." + name + " declaration absent");
        return found->offset;
    };
    // IL2CPP field offsets include the boxed object's 16-byte header. The
    // qualified x64 value parameter is a 48-byte native segment, not a box.
    struct Field {
        const char* name;
        const char* type;
        std::size_t native_offset;
    };
    PartnerControlMethods out;
    out.component_owner = offset(components, "GameComponentBase", "_OwnerRef", "RPG.GameCore.GameEntity");
    auto layout = [&](const auto& rows, const char* owner, std::initializer_list<Field> expected) {
        try {
            for (const auto& field : expected)
                if (offset(rows, owner, field.name, field.type) != field.native_offset + 16)
                    throw std::invalid_argument(std::string(owner) + " value layout differs: " + field.name);
            return true;
        } catch (const std::exception& error) {
            out.binding_errors.push_back(error.what());
            return false;
        }
    };
    const bool attach_layout =
        layout(attachment, "GDBHCNLLPAG", {{"ILDAAHNKMHL", "System.String", 0},
                                         {"IGKONILBPEO", "RPG.GameCore.GameEntity", 8},
                                         {"HFLDHJHJNCF", "System.String", 16},
                                         {"BDNAGGIPGGN", "System.Boolean", 24},
                                         {"FEDLPCHHOEG", "System.Boolean", 25},
                                         {"KPNGMCPOMHJ", "System.Int32", 28},
                                         {"KPAAEDABMDD", "RPG.GameCore.CharacterVisiblityFollowFlag", 32},
                                         {"FCKCHPNEEKB", "RPG.GameCore.GameEntity", 40}});
    const bool visibility_layout =
        layout(visibility, "ANACIPLIMDE", {{"KDOLHICFKLL", "System.String", 0},
                                         {"LPEFHLMHLPN", "System.Boolean", 8},
                                         {"ONMFCPIJGNP", "System.Boolean", 9},
                                         {"IEMGPGLKIIG", "System.Boolean", 10}});
    detail::ResolveBatch bindings(m);
    out.component = bindings.Resolve("RPG.GameCore", "GameEntity", "GetComponent", "RPG.GameCore.GameComponentBase",
                                     {"System.Type"});
    out.owner_world = bindings.Resolve("RPG.GameCore", "GameEntity", "get_OwnerWorldRef", "RPG.GameCore.GameWorld", {});
    out.member_name = bindings.Resolve("", "OLABFALNLJM", "set_MemberName", "System.Void", {"System.String"});
    out.group_leader = bindings.Resolve("", "OLABFALNLJM", "get_Leader", "RPG.GameCore.GameEntity", {});
    if (attach_layout)
        out.attach = bindings.Resolve("RPG.GameCore", "TransformComponent", "AttachTo", "System.Void", {"GDBHCNLLPAG"});
    out.cancel_smooth_follow =
        bindings.Resolve("RPG.GameCore", "TransformComponent", "CancelSmoothFollow", "System.Void", {});
    if (!out.cancel_smooth_follow.code)
        out.attach = {}; // Attach includes the subsequent smooth-follow cancellation.
    out.animation_follow = bindings.Resolve("", "EAKGBLEMBLN", "GBJGAIBJLBB", "System.Void", {"RPG.GameCore.GameEntity"});
    out.visibility_follow =
        bindings.Resolve("RPG.GameCore", "CharacterVisibleComponent", "SetFollowTargetVisibility", "System.Void",
                         {"RPG.GameCore.GameEntity", "RPG.GameCore.CharacterVisiblityFollowFlag"});
    if (visibility_layout)
        out.visibility = bindings.Resolve("RPG.GameCore", "CharacterVisibleComponent", "SetEntityVisibleParam",
                                          "System.Void", {"System.Boolean", "ANACIPLIMDE&"});
    out.idle_animation =
        bindings.Resolve("", "CFFPEAALCPH", "ENBOEHDHCAL", "System.Void",
                         {"RPG.GameCore.AnimStateLogicType", "System.String", "System.Single", "System.Single",
                          "System.Single", "RPG.GameCore.AnimStateLogicTypeExtra", "System.UInt32", "System.Boolean"});
    out.binding_errors.insert(out.binding_errors.end(), bindings.Errors().begin(), bindings.Errors().end());
    return out;
}
}
