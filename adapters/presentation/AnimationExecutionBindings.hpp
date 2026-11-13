#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <array>
#include <optional>
#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
#include <functional>
namespace hksr::api::detail {
using ExecutionFields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
Method ResolveAnimationConstructionEntry(MethodResolver&);
Method ResolveAnimationTaskBuilder(MethodResolver&);
Method ResolveTaskCaster(MethodResolver&);
Method ResolveAnimationRecordDisposal(MethodResolver&);
struct AnimationSequenceLayout {
    std::array<std::size_t, 2> contexts{}, states{};
    std::array<bool, 2> supported{};
    std::vector<std::string> errors;
};
struct AnimationSequenceEntries {
    std::array<void*, 7> methods{};
    std::array<bool, 2> supported{};
    std::vector<std::string> errors;
};
AnimationSequenceLayout ResolveAnimationSequenceLayout(const ExecutionFields&);
AnimationSequenceEntries ResolveAnimationSequenceEntries(MethodResolver&);
AnimationSequenceKind AnimationSequenceEntryKind(std::size_t logical);
bool IsAnimationSequenceRunning(std::int32_t state) noexcept;
std::optional<AnimationSequenceKind> IdentifyAnimationSequence(const RuntimeTypeName&);
}
