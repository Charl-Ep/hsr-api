#include "AnimationExecutionBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Offset(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace.empty() && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::invalid_argument("sequence declaration differs");
            found = &f;
        }
    if (!found)
        throw std::invalid_argument(std::string("sequence field absent: ") + owner + "." + name);
    return found->offset;
}
}
Method ResolveAnimationConstructionEntry(MethodResolver& methods) {
    return methods.Resolve("", "HHECHLOIEJO", "FGAHFIBMPCP", "System.Void",
                           {"RPG.GameCore.SingleAnimEventConfig[]", "RPG.GameCore.SingleAnimBehaviourEvent[]",
                            "RPG.GameCore.AnimGroupEventConfig[]", "RPG.GameCore.LogicAnimEventEntry[]",
                            "RPG.GameCore.TaskContext", "LDMKDJJLOIL", "System.String"});
}
Method ResolveAnimationTaskBuilder(MethodResolver& methods) {
    return methods.Resolve(
        "", "JKNLHJLPIOO", "KJCNEJALIJO", "GELIALJDFBL[]",
        {"RPG.GameCore.TaskContext", "System.Collections.Generic.IEnumerable<RPG.GameCore.TaskConfig>", "BNBOAMEBAOE"},
        MethodKind::Static);
}
Method ResolveTaskCaster(MethodResolver& methods) {
    return methods.Resolve("RPG.GameCore", "TaskContext", "get_CasterEntity", "RPG.GameCore.GameEntity", {});
}
Method ResolveAnimationRecordDisposal(MethodResolver& methods) {
    return methods.Resolve("", "LDMKDJJLOIL", "Dispose", "System.Void", {});
}
AnimationSequenceLayout ResolveAnimationSequenceLayout(const ExecutionFields& fields) {
    AnimationSequenceLayout out;
    const char* types[] = {"KFELGAIGHAP", "MKDHBHBFEDD"};
    const char* context_names[] = {"<TaskContext>k__BackingField", "DDDNLILNMKA"};
    for (std::size_t i = 0; i < out.contexts.size(); ++i) {
        try {
            const auto declarations = fields("", types[i]);
            out.contexts[i] = Offset(declarations, types[i], context_names[i], "RPG.GameCore.TaskContext");
            out.states[i] = Offset(declarations, "GELIALJDFBL", "DELMCJFBNBL", "RPG.GameCore.TaskState");
            out.supported[i] = true;
        } catch (const std::exception& e) {
            out.errors.push_back(std::string(types[i]) + ": " + e.what());
        }
    }
    return out;
}
AnimationSequenceKind AnimationSequenceEntryKind(std::size_t logical) {
    if (logical >= 7)
        throw std::out_of_range("animation sequence entry");
    return logical < 4 ? AnimationSequenceKind::Persistent : AnimationSequenceKind::Immediate;
}
AnimationSequenceEntries ResolveAnimationSequenceEntries(MethodResolver& methods) {
    AnimationSequenceEntries out;
    const char* types[] = {"KFELGAIGHAP", "MKDHBHBFEDD"};
    const char* names[] = {"OnTaskBegin", "Tick", "OnTaskReset", "Dispose", "OnTaskBegin", "OnTaskReset", "Dispose"};
    for (std::size_t kind = 0; kind < out.supported.size(); ++kind) {
        try {
            for (std::size_t logical = 0; logical < out.methods.size(); ++logical)
                if (static_cast<std::size_t>(AnimationSequenceEntryKind(logical)) == kind)
                    out.methods[logical] =
                        logical == 1
                            ? methods.Resolve("", types[kind], names[logical], "System.Void", {"System.Single"}).code
                            : methods.Resolve("", types[kind], names[logical], "System.Void", {}).code;
            out.supported[kind] = true;
        } catch (const std::exception& e) {
            for (std::size_t logical = 0; logical < out.methods.size(); ++logical)
                if (static_cast<std::size_t>(AnimationSequenceEntryKind(logical)) == kind)
                    out.methods[logical] = nullptr;
            out.errors.push_back(std::string(types[kind]) + ": " + e.what());
        }
    }
    return out;
}
bool IsAnimationSequenceRunning(std::int32_t state) noexcept {
    return state == 0x8888;
}
std::optional<AnimationSequenceKind> IdentifyAnimationSequence(const RuntimeTypeName& type) {
    if (!type.name_space.empty())
        return {};
    if (type.name == "KFELGAIGHAP")
        return AnimationSequenceKind::Persistent;
    if (type.name == "MKDHBHBFEDD")
        return AnimationSequenceKind::Immediate;
    return {};
}
}
