#include "../../../adapters/presentation/TimelineTaskBindings.hpp"
#include "../../../adapters/presentation/TimelineRuntimeBindings.hpp"
#include "../../../adapters/presentation/EffectObservationBindings.hpp"
#include "../../../adapters/presentation/AnimationExecutionBindings.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
using namespace hksr::api::detail;
void Check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Resolver final : MethodResolver {
    std::string missing_type, missing_method;
    Method Resolve(const char*, const char* type, const char* method, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        if (type == missing_type && method == missing_method)
            throw std::runtime_error("missing declaration");
        return {this, this};
    }
};
std::vector<DeclaredField> Fields(const char*, const char* type) {
    return {{"", type, "DDDNLILNMKA", "RPG.GameCore.TaskContext", 32, 6, true},
            {"RPG.GameCore", "ImmediateTaskBase`1", "_TaskContext", "RPG.GameCore.TaskContext", 40, 6, true}};
}
void OnlyTaskUnavailable(const TimelineTaskBindings& value, TimelineTask missing) {
    for (std::size_t i = 0; i < value.supported.size(); ++i)
        Check(value.supported[i] == (i != static_cast<std::size_t>(missing)), "unrelated task disabled");
    for (std::size_t i = 0; i < value.entries.size(); ++i)
        Check(bool(value.entries[i].code) == (TimelineEntryTask(i) != missing), "sparse logical entry mapping changed");
    Check(value.errors.size() == 1, "missing contract diagnostic");
}
}
int main() {
    try {
        Resolver methods;
        const auto complete = ResolveTimelineTaskBindings(methods, Fields);
        Check(complete.errors.empty(), "complete declarations rejected");
        Check(std::all_of(complete.supported.begin(), complete.supported.end(), [](bool x) { return x; }),
              "complete task unsupported");
        methods.missing_type = "JODBIGDDMOB";
        methods.missing_method = "Tick";
        OnlyTaskUnavailable(ResolveTimelineTaskBindings(methods, Fields), TimelineTask::Wait);
        methods.missing_type = "OKKFJLFMPFK";
        methods.missing_method = "OnSkip";
        OnlyTaskUnavailable(ResolveTimelineTaskBindings(methods, Fields), TimelineTask::Play);
        methods.missing_type.clear();
        methods.missing_method.clear();
        OnlyTaskUnavailable(ResolveTimelineTaskBindings(methods,
                                                        [](const char* ns, const char* type) {
                                                            if (std::string(type) == "MPKLOFIMGGL")
                                                                return std::vector<DeclaredField>{};
                                                            return Fields(ns, type);
                                                        }),
                            TimelineTask::EffectConfigGFJHLACJMNK);
        Check(TimelineTaskRetainsCycle(TimelineTask::Play) && !TimelineTaskRetainsCycle(TimelineTask::Stop),
              "task lifetime classification changed");
        const auto waits = ResolveTimelineWaitBindings([](const char*, const char* type) {
            if (std::string(type) == "JODBIGDDMOB")
                return std::vector<DeclaredField>{};
            return std::vector<DeclaredField>{{"", type, "BMHGECJDFGN", "EBMMGODFEEO", 72, 6, true}};
        });
        Check(!waits[0].available && !waits[0].error.empty() && waits[1].available && waits[1].offset == 72,
              "one wait schema disabled both wait contracts");
        methods.missing_type = "JCNLLEPCBGI";
        methods.missing_method = "NMIPDBNGLBH";
        const auto scopes = ResolveEffectScopeEntries(methods);
        Check(scopes.supported[0] && scopes.supported[1] && !scopes.supported[2],
              "camera declaration disabled normal effect attribution");
        Check(scopes.methods[3].code && scopes.methods[4].code && !scopes.methods[5].code && !scopes.methods[6].code,
              "effect sparse logical slots shifted");
        methods.missing_type = "NFGENDDOCJJ";
        methods.missing_method = "OnTaskBegin";
        const auto submissions = ResolveEffectSubmissionEntries(methods);
        Check(submissions.methods[0] && submissions.methods[1] && !submissions.methods[2],
              "task origin declaration disabled direct effect submissions");
        const auto submission_fields = ResolveEffectSubmissionBindings(methods, [](const char* ns, const char* type) {
            if (std::string(type) != "TriggerEffectParams")
                return std::vector<DeclaredField>{};
            return std::vector<DeclaredField>{
                {ns, type, "<UniqueEffectID>k__BackingField", "System.UInt32", 28, 6, true},
                {ns, type, "CasterEntity", "RPG.GameCore.GameEntity", 40, 6, true}};
        });
        Check(submission_fields.id_offset == 28 && submission_fields.caster_offset == 40 &&
                  !submission_fields.trigger_error.empty(),
              "missing task origin layout disabled manager request receipt");
        methods.missing_type = "KFELGAIGHAP";
        methods.missing_method = "Dispose";
        const auto sequences = ResolveAnimationSequenceEntries(methods);
        Check(!sequences.supported[0] && sequences.supported[1] && !sequences.methods[0] && sequences.methods[4],
              "persistent failure disabled immediate sequences");
        const auto sequence_layout = ResolveAnimationSequenceLayout([](const char*, const char* type) {
            if (std::string(type) == "KFELGAIGHAP")
                return std::vector<DeclaredField>{};
            return std::vector<DeclaredField>{
                {"", type, "DDDNLILNMKA", "RPG.GameCore.TaskContext", 32, 6, true},
                {"", "GELIALJDFBL", "DELMCJFBNBL", "RPG.GameCore.TaskState", 48, 6, true}};
        });
        Check(!sequence_layout.supported[0] && sequence_layout.supported[1],
              "missing persistent layout disabled immediate layout");
        std::cout << "timeline partial binding tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
