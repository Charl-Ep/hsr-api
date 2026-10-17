#pragma once
#include "../metadata/RuntimeMetadata.hpp"
#include "../managed/ManagedArrays.hpp"
#include "../unity/ComponentConfig.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct NpcCatalogRow { std::uint32_t id=0; std::string character_config,model_config; };
struct PlayerCatalogRow {
    std::uint32_t id=0,avatar=0,enhanced=0,skin=0;
    std::string character_config,prefab;
};
struct AvatarCatalogRow { std::uint32_t avatar=0; std::string character_config,prefab; };
struct CatalogIssue { std::int32_t index=-1; std::string operation,error; };
template<class T> struct CatalogBatch {
    std::vector<T> rows;
    std::int32_t declared_count=0,visited_count=0;
    bool enumerated=false;
    std::vector<CatalogIssue> issues;
};
struct CharacterCatalog {
    CatalogBatch<NpcCatalogRow> npcs;
    CatalogBatch<PlayerCatalogRow> players;
    CatalogBatch<AvatarCatalogRow> avatars;
};
struct TextInput { bool available=false; std::string value,error; };
struct TextArrayInput { bool available=false; std::vector<std::string> values; std::string error; };
struct PresentationConfigInput {
    std::string path;
    TextInput initial_state;
    TextArrayInput event_files,disabled_event_layers;
};
template<class T> struct ConfigValue { bool available=false; T value{}; std::string error; };
enum class ActionConfigKind { Adventure, Battle, AnimationEvent, Elation };
struct ConfigReference { std::string value,field; };
struct SkillReadyVariant {
    std::int32_t source_index=0;
    TextInput state;
    ConfigValue<bool> has_condition;
    ConfigValue<std::int32_t> priority;
};
struct SkillInput {
    std::int32_t source_index=0;
    TextInput name,ready_state,entry_ability,prepare_ability,ready_effect;
    ConfigValue<std::int32_t> type;
    ConfigValue<bool> use_transition,cut_1_to_2,cut_2_to_1,wait_prepare,auto_standby;
    ConfigValue<float> ready_wait;
    TextArrayInput child_skills;
    CatalogBatch<SkillReadyVariant> variants;
};
struct ReadyTransitionInput {
    std::int32_t source_index=0;
    ConfigValue<std::int32_t> from,to;
    TextInput state;
    ConfigValue<float> transition_in,reverse_outro;
};
struct ElationReadyInput {
    std::int32_t source_index=0;
    TextInput skill,entry,loop;
};
struct ReadyAnimationDefaults {
    ConfigValue<float> transition_in;
    // Official helper constants, read once as values; not per-character maps.
    TextInput default_expression,empty_layer_state,hit_additive_layer,hit_override_layer;
    // Official fallback domain qualified for this adapter, not a skill-count cap.
    std::vector<ReadyTransitionInput> transitions;
};
// One configuration document, not an Animator state inventory or an executable
// skill graph. Every literal retains its source field. Numeric categories and
// times are source values, not menu indices or inferred phase semantics.
struct ActionConfigInput {
    ActionConfigKind kind=ActionConfigKind::Adventure;
    std::string path;
    bool loaded=false;
    PresentationConfigInput presentation;
    TextInput referenced_event_file;
    CatalogBatch<SkillInput> skills;
    CatalogBatch<ReadyTransitionInput> transitions;
    std::vector<ConfigReference> states,abilities;
    std::vector<CatalogIssue> issues;
    TextInput elation_config;
    CatalogBatch<ElationReadyInput> elation_ready;
};
// Source declarations, not gameplay modifiers and not a name-based emotion map.
// A missing field stays unavailable; an empty expression is a valid source value.
struct VisualPhaseInput {
    TextInput state;
    // Original category fields may still be uninitialized. Preserve them apart
    // from categories produced by the official preparation on a private copy.
    ConfigValue<std::uint8_t> source_category;
    // Prepared nonzero category overrides the literal name: official reaction
    // lookup, optional character phase, then default. No controller execution.
    TextInput resolved_state;
    ConfigValue<float> transition;
    ConfigValue<std::uint8_t> category;
};
struct VisualReactionInput {
    bool present=false;
    TextInput hit,hit_h,hit_fly,hit_fly_recover,fly_hit,die,die_hit,die_fly,die_limbo;
    ConfigValue<bool> force_hit_h;
    ConfigValue<float> hit_weight;
};
struct VisualBehaviorInput {
    std::int32_t source_index=0;
    bool prepared=false;
    ConfigValue<std::int32_t> flag,material_effect;
    TextInput priority,emotion,floating_fail_recover;
    VisualPhaseInput occur,loop,recover;
    VisualReactionInput reactions;
    ConfigValue<float> speed,speed_transition,speed_range,repeat_occur_at,material_transition;
    ConfigValue<float> floating_height,floating_rise,floating_fall;
    ConfigValue<bool> force_hit_h,disable_look_at,forbid_special_standby,forbid_special_character;
    ConfigValue<bool> only_player,ignore_anim_config;
    std::vector<CatalogIssue> issues;
};
struct VisualConfigInput {
    std::string source;
    bool loaded=false,phase_present=false;
    CatalogBatch<VisualBehaviorInput> behaviors;
    std::vector<CatalogIssue> issues;
};
// Both tables resolved against the SAME selected base character phase. Do not
// merge separately resolved default rows: their category references may need
// this character's reaction overrides even when it has no replacement row.
struct VisualConfigSources {
    VisualConfigInput defaults,character;
    std::string character_config;
    bool context_available=false;
    // Current qualified 4.6 CFF Visual request uses -1; its expression selector
    // substitutes the native 0.2 normalized transition, NOT body phase duration.
    ConfigValue<float> expression_transition;
    TextInput blink_suppressed_expression;
    ConfigValue<int> blink_source;
};
enum class ConfigSelection { Reference, Index, StringKey };
struct ConfigStep {
    std::string declaring_type,field;
    ConfigSelection selection=ConfigSelection::Reference;
    std::int32_t index=-1;
    std::string key;
};
// An explicitly selected CharacterPhaseAnimConfig inside an official modifier.
// The path selects data only; callbacks and tasks are never executed.
struct ModifierPhaseSource {
    std::string modifier;
    std::vector<ConfigStep> path;
};
struct PhaseVisualConfigSources {
    ModifierPhaseSource source;
    VisualConfigSources visuals;
    TextInput standby,revive;
    VisualReactionInput reactions;
    std::vector<CatalogIssue> issues;
};
enum class AnimationTaskKind { State, StateWithMove, StateWithTargetList };
struct AnimationTaskSource {
    std::string ability;
    std::vector<ConfigStep> path;
    AnimationTaskKind kind=AnimationTaskKind::State;
    TextInput state;
    ConfigValue<std::int32_t> logic_kind;
    ConfigValue<bool> enabled,waits;
    TextInput target_alias;
};
// Literal declarations only. These are not an executable skill graph; in
// particular, the reader does not evaluate DynamicFloat or branch predicates.
struct AnimationWaitSource {
    std::string ability;
    std::vector<ConfigStep> path;
    TextInput state,target_alias;
    ConfigValue<bool> enabled;
};
struct EffectRemovalInput {
    TextInput effect_path,path_key,target_alias;
    ConfigValue<bool> enabled;
};
struct EffectRemovalSource {
    std::string ability;
    std::vector<ConfigStep> path;
    EffectRemovalInput input;
};
struct AbilitySourceIssue { std::string ability,error; std::vector<ConfigStep> path; };
struct AbilityReferenceSource {
    std::string ability;
    std::vector<ConfigStep> path;
    TextInput target;
    ConfigValue<bool> enabled;
};
struct SkillMappingSource {
    std::string ability;
    std::vector<ConfigStep> path;
    TextInput skill,target_alias;
    ConfigValue<std::int32_t> control;
    ConfigValue<bool> enabled;
};
struct SkillContinuationSource {
    std::string ability;
    std::vector<ConfigStep> path;
    ConfigValue<std::int32_t> control,child;
    ConfigValue<bool> enabled;
};
struct AbilitySources {
    std::vector<AnimationTaskSource> animations;
    std::vector<AbilitySourceIssue> issues;
    std::size_t nodes_read=0;
    std::vector<AnimationWaitSource> waits;
    std::vector<EffectRemovalSource> removals;
    std::vector<AbilityReferenceSource> references;
    std::vector<SkillMappingSource> skill_mappings;
    std::vector<SkillContinuationSource> skill_continuations;
};
// Engine-thread cursor over one batch. Budget is a yield target, never a result
// count cap: a single official call cannot be preempted. Cancel stops discovery
// and Advance drains retained references before reporting Done. Destruction is
// an engine-thread fallback for shutdown; callers normally drain to completion.
class AbilitySourceRead {
public:
    virtual ~AbilitySourceRead()=default;
    virtual void Advance(std::uint32_t budget_microseconds)=0;
    virtual void Cancel()=0;
    virtual bool Done() const=0;
    virtual AbilitySources Take()=0; // only after Done; cancelled reads return no partial catalog
};
struct CompletedAbilitySources {
    std::vector<std::string> abilities; // fully traversed roots, including failed/empty roots
    AbilitySources sources;
};
// Optional streaming interface; the base reader's vtable remains unchanged.
// DrainCompleted transfers complete roots only. Take returns any undrained
// results after Done. Published values contain no managed references; the
// reader retains its shared node cache until completion/cancellation drains it.
class ProgressiveAbilitySourceRead : public AbilitySourceRead {
public:
    virtual CompletedAbilitySources DrainCompleted()=0;
};
struct NpcMotionRow {
    std::uint32_t id=0;
    std::string indexed_name;
    TextInput name,source_character,start_path,loop_path;
};
struct NpcMotionIndex {
    std::string character;
    CatalogBatch<NpcMotionRow> motions;
};
struct NpcMotionSources {
    NpcMotionIndex index;
    std::string character_config,state_config;
    bool state_list_read=false;
    std::vector<std::uint32_t> configured_ids;
    std::vector<NpcMotionRow> motions; // union, deduplicated by actual motion ID
    std::vector<CatalogIssue> issues;
};
// Engine-thread, read-only official data access. Returned values contain no
// game pointers. It neither creates actors nor loads model prefabs/tasks.
// One table pass per ReadCharacters call; the caller owns caching/reload policy.
// Configurations are explicit, on-demand requests, not a full eager sweep.
class HKSR_API OfficialCatalog {
public:
    OfficialCatalog(MethodResolver& methods,ObjectMetadata& metadata,ManagedObjects& objects,
        ReferenceMethods references,ManagedArrays arrays,ComponentConfig text);
    ~OfficialCatalog();
    OfficialCatalog(const OfficialCatalog&)=delete;
    OfficialCatalog& operator=(const OfficialCatalog&)=delete;
    CharacterCatalog ReadCharacters();
    TextInput ReadAdventureIdentity(const std::string& path);
    PresentationConfigInput ReadPresentationConfig(const std::string& path);
    // Reads one explicit document. ReferenceJsonConfig/event lists are returned
    // as values; caller controls traversal/cache. No task execution or prefab load.
    ActionConfigInput ReadActionConfig(ActionConfigKind kind,const std::string& path);
    // Official fallback query only, no skill/component construction or playback.
    // Arguments are zero-based runtime skill indices; not SkillType, UI index,
    // or positions in a filtered menu. Null official result is a valid empty name.
    TextInput ReadDefaultReadyTransition(std::int32_t from_runtime_index,std::int32_t to_runtime_index);
    ReadyAnimationDefaults ReadReadyAnimationDefaults();
    // Named original data, available without constructing a battle or its cache.
    // Prepares private shallow copies with the official per-row function. No
    // source edits, global table registration, battle construction or flags applied.
    VisualConfigInput ReadDefaultVisualConfig();
    // The base PhaseAnimConfig only. Null means no character override, not error.
    // Selecting later phases is caller policy and is not inferred here.
    VisualConfigInput ReadCharacterVisualConfig(const std::string& character_config);
    VisualConfigSources ReadVisualConfigSources(const std::string& character_config);
    // Resolves both visual tables against the explicitly selected phase. A
    // null final phase uses the character's base phase; an unread path does not.
    // Copies values and prepares private visual rows, without adding components.
    PhaseVisualConfigSources ReadPhaseVisualConfigSources(const std::string& character_config,
        const ModifierPhaseSource&,TypeRelationships&);
    // Automatic task-reference discovery, not execution permission or a phase
    // graph. Traverses only JsonConfig-derived references and boxed sequences;
    // dictionary values require declared reference ancestry before get_Item.
    AbilitySources ReadAbilitySources(const std::vector<std::string>& abilities,TypeRelationships& types);
    // Keeps one shared node cache for the entire batch across frame boundaries.
    // The metadata/type provider must outlive the reader, as for this catalog.
    std::unique_ptr<AbilitySourceRead> BeginAbilitySources(std::vector<std::string> abilities,TypeRelationships& types);
    // The official character/name index, NOT a complete state graph or the
    // union with explicit cross-character references in a FreeStyle state file.
    NpcMotionIndex ReadNpcMotionIndex(const std::string& freestyle_character);
    NpcMotionSources ReadNpcMotions(const std::string& freestyle_character,const std::string& freestyle_character_config);
    // Caller selects its own currently live FreeStyle component. This reader
    // derives both inputs through official getters, not NPC ID/name guessing.
    NpcMotionSources ReadNpcMotionsFromComponent(void* owned_freestyle_component);
    // Operations release their temporary roots. Unknown releases remain here,
    // never repeated or hidden by a successful value snapshot.
    std::size_t UnresolvedReferences() const;
private:
    struct State;
    std::shared_ptr<State>* owner_;
    State* state_;
};
}
