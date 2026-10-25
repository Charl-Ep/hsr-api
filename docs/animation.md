# Animation

## Sessions and inputs

Create a session with `context.Animations()(entity)`. `ActorAnimationSession` owns entity/model access, `AnimationControl`, `ProfileInputs` and `AnimationResources`. Accessors return borrowed services. Use them and finish `Close()` on the entity's official thread.

`AnimationResources` retains the entity generation, component types and temporary strings:

| Operation | Behavior |
|---|---|
| `Component` | Resolve a model, FreeStyle or emotion type and read its current component |
| `CurrentComponent` | Read using an already cached type, or return empty |
| `Text` | Create and retain a UTF-8 string |
| `Drain` | Release temporary strings |
| `Close` | Terminal cleanup of strings, types and owner roots |

Component instances are borrowed and may change during a managed call. Recheck model and intent before subsequent writes. Close during an active operation returns false; finish cleanup after that call unwinds.

`AnimationControl` exposes official Animator, FreeStyle and controller calls. `ResolveStateName` returns borrowed text from the current Animator. `AnimationResources::StateName` retains it, verifies its hash round trip and returns UTF-8.

## Temporary overrides

`AnimationOverrideTarget` borrows the session's resources, model access and animation control. Keep these alive until all overrides are released.

| Override | Capture and restore |
|---|---|
| `ExpressionOverride` | Find a unique secondary layer, capture its effective state/time, then restore while the layer still contains this override |
| `BlinkOverride` | Disable one enabled blink source; restore on the same emotion component |
| `ReadySpeedOverride` | Capture the official transition parameter; restore if it still equals this override's last write |

A replacement expression on the same layer inherits the original baseline. During a transition, capture uses the next state. Official successor states are left intact.

`ExpressionOverride::ReleaseTo` uses a newly resolved return state instead of the captured baseline. It verifies membership in the same layer and preserves a newer expression writer.

Writes are recorded before invocation. Retain an override after a partial failure and release it explicitly. Release is terminal; uncertain restores are not repeated. Destructors perform no engine cleanup. Identity checks reject ended entity generations and replaced Animators; cleanup can still restore the same Animator while its model is changing.

## Ready entry and transitions

`OfficialCatalog::ReadCharacterHitInput` reads resilience, occurrence-repeat timing and the optional base phase's hit fields without enumerating skills or abilities. `ReadDefaultHitNames` queries official reaction names; `ReadHitAnimationDefaults` reads global weight and interval values. Cache these configuration snapshots outside per-frame playback. Runtime phase and behavior selection belong to the consumer.

`ReadReadyAnimationDefaults` includes the official hit-expression name. `AnimationResources::LayerName` reads and retains a current model's layer name. `AnimationComponent::Motion` locates the actor's motion component; `AnimationControl::HitMotionHeight` reads its flight/fall height, not world position. Null or failed observations remain unavailable. `EntityAccess::Type` and `AliveState` read the corresponding raw enums separately from ordinary loading checks.

`PrepareReadyPresentation` resets configured hit layers and the unique default-expression layer. Active hit layers are evaluated at zero delta, moved to the empty state and disabled. The expression uses a 0.2 normalized blend. Missing optional layers are reported independently. The result's `current` flag tracks entity, Animator and caller-intent identity.

`FindReadyTransition` takes zero-based skill indices and ordered declaration snapshots. It converts to authored one-based keys, selects the first character match and falls back to defaults only when the complete character table has no match. A matched empty/unread state suppresses fallback. Unread keys and incomplete tables return distinct results. The returned row owns its timing and source data; the consumer checks Animator membership and selects playback.

## Preparation relations

`OfficialCatalog::ReadTaskConfig(path)` reads one battle character document. Its result retains the action configuration, every skill's use type and boost key, condition/target field provenance, and each `SkillAbilityList` row's owning skill key. Null references, unread fields and empty arrays remain distinct. It does not evaluate conditions or load ability graphs.

`OfficialCatalog::BeginAbilitySources` reads task declarations on the official thread with a per-call time budget. `ProgressiveAbilitySourceRead::DrainCompleted` transfers fully traversed roots before the batch finishes; shared nodes remain cached across roots. `Done` includes reference cleanup, and `Take` returns undrained results. Cancellation discards undelivered values and drains retained references. A single native call cannot be interrupted by the budget.

`FindPreparationSequences` examines each ability's straight `OnStart` list in an `AbilitySources` snapshot. It pairs a continuation with the nearest preceding mapping for the same control slot and retains intervening animation declarations. It preserves flags, targets, child indices and unread fields.

The query does not flatten branches or execute gameplay predicates. Consumers use these declarations to select visual playback; the SDK does not execute complete battle skill graphs.

## Phase visual data

`ReadVisualConfigSources(character_config)` resolves global and character visual rows against the base character phase. `ReadPhaseVisualConfigSources(character_config, source, types)` selects a `CharacterPhaseAnimConfig` from an explicit ability, global modifier or global template definition. `PhaseVisualSource` contains the root kind, definition name and declared field path. Paths support reference fields, array/list indices and string dictionary keys; local definitions are reached through their owner.

The selected phase replaces the base phase as a whole. A null final phase falls back to the base; an unread path reports an error. Results contain copied standby, revive, reaction and visual-state data. Global rows resolve their category references against the same selected phase. Consumers retain the separate global expression source.

Both readers prepare private row copies through the official visual-table helper. They do not modify source configurations, add entity components or execute modifier callbacks. Cache the results outside per-frame playback.
