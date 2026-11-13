#include "TimelineTaskBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
struct TaskType {
    const char* type;
    const char* owner_space;
    const char* owner;
    const char* context;
    bool retains_cycle = false;
};
constexpr std::array<TaskType, static_cast<std::size_t>(TimelineTask::Count)> types{
    {{"OKKFJLFMPFK", "", "OKKFJLFMPFK", "DDDNLILNMKA", true},
     {"MLAPLADNCKE", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"JODBIGDDMOB", "", "JODBIGDDMOB", "DDDNLILNMKA", true},
     {"IGBEIECLHMC", "", "IGBEIECLHMC", "DDDNLILNMKA", true},
     {"INFHMIEOCGH", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"MPKLOFIMGGL", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"BKIDMGGCGKL", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"OCBDDGMOFNF", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"PIJKEKGCHIF", "", "PIJKEKGCHIF", "DDDNLILNMKA", true},
     {"CHGJJPLLGPM", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"DMOBAEFEAEO", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"AOHNNKDMBJA", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"GLDOCBPPCFG", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"BCPLNBHIENH", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"IOKBPCJKEOL", "RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext"},
     {"JCMHNJJLLNE", "", "JCMHNJJLLNE", "DDDNLILNMKA", true}}};
std::size_t ContextOffset(const std::vector<DeclaredField>& fields, const TaskType& type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == type.owner_space && f.owner_name == type.owner && f.name == type.context) {
            if (found || f.type != "RPG.GameCore.TaskContext" || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("Timeline task context declaration differs: ") + type.type);
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("Timeline task context absent: ") + type.type);
    return found->offset;
}
}
namespace {
struct Entry {
    TimelineTask task;
    const char* name;
    bool tick;
};
constexpr std::array<Entry, 32> entries{{{TimelineTask::Play, "OnTaskBegin", false},
                                         {TimelineTask::Stop, "OnTaskBegin", false},
                                         {TimelineTask::Wait, "OnTaskBegin", false},
                                         {TimelineTask::Wait, "Tick", true},
                                         {TimelineTask::Wait, "OnTaskReset", false},
                                         {TimelineTask::Wait, "Dispose", false},
                                         {TimelineTask::WaitFinish, "OnTaskBegin", false},
                                         {TimelineTask::WaitFinish, "Tick", true},
                                         {TimelineTask::WaitFinish, "OnTaskReset", false},
                                         {TimelineTask::WaitFinish, "Dispose", false},
                                         {TimelineTask::EffectSync, "OnTaskBegin", false},
                                         {TimelineTask::Play, "Tick", true},
                                         {TimelineTask::Play, "OnTaskReset", false},
                                         {TimelineTask::Play, "Dispose", false},
                                         {TimelineTask::Play, "OnSkip", false},
                                         {TimelineTask::EffectConfigGFJHLACJMNK, "OnTaskBegin", false},
                                         {TimelineTask::EffectConfigNGLKHNKNBDC, "OnTaskBegin", false},
                                         {TimelineTask::HideEffectAutoLayout, "OnTaskBegin", false},
                                         {TimelineTask::StartEffectAim, "OnTaskBegin", false},
                                         {TimelineTask::StartEffectAim, "Tick", true},
                                         {TimelineTask::StartEffectAim, "OnTaskReset", false},
                                         {TimelineTask::StartEffectAim, "Dispose", false},
                                         {TimelineTask::SetEffectAutoLayoutScale, "OnTaskBegin", false},
                                         {TimelineTask::TransferMahJongLayoutEffect, "OnTaskBegin", false},
                                         {TimelineTask::TriggerAutoLayoutTransferPerform, "OnTaskBegin", false},
                                         {TimelineTask::SetCharacterEffectMaterialBlock, "OnTaskBegin", false},
                                         {TimelineTask::RecordAutoLayoutSnapShot, "OnTaskBegin", false},
                                         {TimelineTask::SetEffectFrameCaptureMatTex, "OnTaskBegin", false},
                                         {TimelineTask::StopEffectFollow, "OnTaskBegin", false},
                                         {TimelineTask::StopEffectFollow, "Tick", true},
                                         {TimelineTask::StopEffectFollow, "OnTaskReset", false},
                                         {TimelineTask::StopEffectFollow, "Dispose", false}}};
}
TimelineTaskBindings ResolveTimelineTaskBindings(MethodResolver& m, const TimelineRegistryAccess::Fields& fields) {
    TimelineTaskBindings out;
    out.caster = m.Resolve("RPG.GameCore", "TaskContext", "get_CasterEntity", "RPG.GameCore.GameEntity", {});
    for (std::size_t i = 0; i < types.size(); ++i) {
        const auto task = static_cast<TimelineTask>(i);
        try {
            out.contexts[i] = ContextOffset(fields("", types[i].type), types[i]);
            for (std::size_t j = 0; j < entries.size(); ++j)
                if (entries[j].task == task) {
                    const auto& e = entries[j];
                    out.entries[j] = e.tick ? m.Resolve("", types[i].type, e.name, "System.Void", {"System.Single"})
                                            : m.Resolve("", types[i].type, e.name, "System.Void", {});
                }
            out.supported[i] = true;
        } catch (const std::exception& e) {
            // A task and its paired lifecycle observations form one contract.
            // No callback for it is installed without complete provenance and
            // cleanup coverage; unrelated task contracts remain available.
            for (std::size_t j = 0; j < entries.size(); ++j)
                if (entries[j].task == task)
                    out.entries[j] = {};
            out.errors.push_back(std::string(types[i].type) + ": " + e.what());
        }
    }
    return out;
}
TimelineTask TimelineEntryTask(std::size_t index) {
    return entries.at(index).task;
}
bool TimelineTaskRetainsCycle(TimelineTask task) {
    return types.at(static_cast<std::size_t>(task)).retains_cycle;
}
}
