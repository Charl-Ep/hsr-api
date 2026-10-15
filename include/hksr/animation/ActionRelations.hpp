#pragma once
#include "OfficialCatalog.hpp"
#include <optional>

namespace hksr::sdk_v1 {
enum class ReadyTransitionTable { None, Character, OfficialDefault };
enum class ReadyTransitionResolution { Found, Absent, UnreadKey, IncompleteTable, InvalidIndex };
struct ReadyTransitionMatch {
    ReadyTransitionResolution resolution=ReadyTransitionResolution::Absent;
    ReadyTransitionTable table=ReadyTransitionTable::None;
    std::size_t position=0;
    // Owned copy, including unavailable fields and source_index. Found with an
    // empty/unread state is NOT absence and must not trigger default fallback.
    std::optional<ReadyTransitionInput> row;
};
// Pure query over ordered declaration snapshots. Skill indices are zero-based;
// authored transition keys are one-based. First match wins. Character defaults
// are considered only after a complete character table has no matching row.
// This does not decide whether to use transitions, sanitize timings, choose a
// direct pose, test Animator membership, or execute any game operation.
HKSR_API ReadyTransitionMatch FindReadyTransition(
    const CatalogBatch<ReadyTransitionInput>& character,
    const std::vector<ReadyTransitionInput>& defaults,
    int from_skill_index,int to_skill_index);

struct PreparationSequence {
    SkillMappingSource mapping;
    SkillContinuationSource continuation;
    std::vector<AnimationTaskSource> animations;
};
// Structural evidence, not a skill interpreter or an inferred ready-state map.
// Within one ability's straight OnStart list, pair a continuation with its
// nearest preceding mapping of the same readable control slot, and retain all
// intervening animation declarations. Enabled/target/state/child values remain
// untouched. Nested branches are not flattened; predicates are not evaluated.
// The caller still owns AbilitySources::issues and decides preview eligibility.
HKSR_API std::vector<PreparationSequence> FindPreparationSequences(const AbilitySources& sources);
}
