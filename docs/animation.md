# Animation

## Catalog identity

`ReadInstalledCatalogRevision()` reads the installed data identity on a worker.
`OfficialCatalog::ReadCatalogRevision(installed)` combines it with the current
official data selection on the engine thread. Consumers use this opaque key to
invalidate cached catalog values. Capability qualification is independent.

## Character display names

`OfficialCatalog::ReadTextLanguage()` reads the game's current raw text-language
key, for example `cn` or `en`. Consumers map these keys to their own language tags.
`ReadCharacterDisplayName(kind, id)` resolves one NPCData ID, avatar ID or skin ID
in that language. Player results include the path and default appearance name.
Each field reports availability independently; an available empty string is valid.

The reads use the current official text table and resolve only the requested
display identity. Consumers refresh cached values when the official language changes.

## Sessions and inputs

Create a session with `context.Animations()(entity)`. `ActorAnimationSession` owns entity/model access, `AnimationControl`, `ProfileInputs` and `AnimationResources`. Accessors return borrowed services. Use them and finish `Close()` on the entity's official thread.

Animation and control sessions accept an explicit entity epoch. The caller establishes ownership; component-creation and scoped-resource factories additionally enforce their documented client-ownership requirements.

Low-level providers pass these call services directly to the constructor. Optional
`ActorHitGeometryBindings` supplies geometry, direction, placement and scene reads.
Spatial queries return unavailable when their required bindings are absent.
Direction and effect-placement queries have independent binding requirements.
Closed or ended sessions return unavailable, including closure during a query.

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

`AnimatorCatalog::UnresolvedReferences()` reports unconfirmed temporary-reference cleanup separately from directory completeness. Unknown GC release outcomes are retained and never retried.

## Actor parameters

`ActorAnimationSession::SetParameter(world, parameter, propagation)` accepts a named Float, Int, Bool or `AnimationTrigger`. `AnimationTrigger::reset=true` clears the trigger; false sets it. Sessions created by `PresentationContext::Animations` write through the actor's official `MockAnimator`, including its parameter-recording path.

`PresentationContext::Prepare` resolves reusable bindings. Per-command calls use the current actor, model and effect objects. Missing propagation bindings leave `BodyOnly` available.

`BodyOnly` writes the body. `SyncPropState` also writes every Animator returned by associated effects with `SyncPropState` enabled and a creation/request receipt owned by the same actor and client. Animation events retain their normal sounds, attachments and effects.

The call is bound to the actor generation, world and current model/Art/Animator/MockAnimator identities. Model replacement ends propagation; a later call binds the new model. Run on the owning engine thread. `Close()` disables further session writes.

`AnimationParameterResult` reports the body call, effect write/skip/failure counts and temporary-reference cleanup. `Applied` confirms the synchronous setter call. A failed propagation may leave a successful body write (`Partial`). Callers restore values explicitly; triggers have a reset operation. Writes apply to effects present at submission.

## Actor timing

`PresentationContext::CreateActorTiming(entity, world)` creates an engine-thread timing session. Metadata bindings are prepared once by `PresentationContext::Prepare`; each session retains its own actor/world inputs.

| Operation | Contract |
|---|---|
| `Layer` / `SetLayer` | Read a named layer and blend its weight through the official component over the supplied duration |
| `Speed` / `SetSpeed` | Read/write the battle component's animation lock speed; `-1` resets the override |
| `Scale` | Add an actor `TimeScaleStack` entry with an explicit key, factor, delay, fades and optional finite duration |
| `Contains` / `Remove` | Observe or retire the exact stack/index receipt; fading removal can remain active |
| `Close` | Retire this session's clock receipts and release references |

Layer/speed writes bind the current model, Art, Animator, controller and component. Callers coordinate these single-slot writes and restore them before closure. Actor clocks bind the actor/stack and survive model changes. The game advances animation and fades; this service controls actor-local inputs.

## Temporary overrides

`AnimationOverrideTarget` borrows the session's resources, model access and animation control. Keep these alive until all overrides are released.

| Override | Capture and restore |
|---|---|
| `ExpressionOverride` | Find a unique secondary layer, capture its effective state/time, then restore while the layer still contains this override |
| `BlinkOverride` | Disable one enabled blink source; restore on the same emotion component |
| `ReadySpeedOverride` | Capture the official transition parameter; restore if it still equals this override's last write |

A replacement expression on the same layer inherits the original baseline. During a transition, capture uses the next state. Official successor states are left intact.

`ExpressionOverride::ReleaseTo` returns to an explicitly resolved state in the same layer, preserving a newer expression writer.

Writes are recorded before invocation. Retain an override after a partial failure and release it explicitly. Release is terminal; uncertain restores are not repeated. Destructors perform no engine cleanup. Identity checks reject ended entity generations and replaced Animators; cleanup can still restore the same Animator while its model is changing.

## Ready entry and transitions

`OfficialCatalog::ReadCharacterHitInput` reads resilience, occurrence-repeat timing and the optional base phase's hit fields. `ReadDefaultHitNames` queries official reaction names; `ReadHitAnimationDefaults` reads global weight and interval values. Cache these configuration snapshots outside per-frame playback. Runtime phase and behavior selection belong to the consumer.

`ReadReadyAnimationDefaults` includes the official hit-expression name. `AnimationResources::LayerName` reads and retains a current model's layer name. `AnimationComponent::Motion` locates the actor's motion component; `AnimationControl::HitMotionHeight` reads its flight/fall height. Null or failed observations remain unavailable. `EntityAccess::Type` and `AliveState` read the corresponding raw enums separately from loading checks.

`ReadCharacterHitInput` also returns base hit-shape dimensions, offset and attachment name. `HitGeometry` projects these inputs using the current logical transform or resolved attachment. `HitDirection::WorldDirection` uses the official `_CalcHitDir`; `ActorAnimationSession::ReadHitAngle` combines geometry, the model transform and mirror flag. The consumer supplies configuration overrides and any random angle contribution. `AnimationControl::HitAngleParameter` resolves the key for a separate Animator write. These are geometry and parameter queries.

`HitGeometry::EffectOffset` computes the target-local effect point before effect overrides and scale adjustment. Negative height selects the default local offset. Effect creation and lifetime are separate services.

`PrepareReadyPresentation` resets configured hit layers and the unique default-expression layer. Active hit layers are evaluated at zero delta, moved to the empty state and disabled. The expression uses a 0.2 normalized blend. Missing optional layers are reported independently. The result's `current` flag tracks entity, Animator and caller-intent identity.

`FindReadyTransition` takes zero-based skill indices and ordered declaration snapshots. It converts to authored one-based keys, selects the first character match and falls back to defaults only when the complete character table has no match. A matched empty/unread state suppresses fallback. Unread keys and incomplete tables return distinct results. The returned row owns its timing and source data; the consumer checks Animator membership and selects playback.

## Preparation relations

`OfficialCatalog::ReadTaskConfig(path)` reads one battle character document. Its result retains the action configuration, every skill's use type and boost key, condition/target field provenance, and each `SkillAbilityList` row's owning skill key. Null references, unread fields and empty arrays remain distinct. Evaluate conditions and load referenced ability graphs separately.

`OfficialCatalog::BeginAbilitySources` returns an `AbilitySourceRead` cursor with a per-call time budget. `DrainCompleted` transfers fully traversed roots once; it may return empty before `Done`. Shared nodes remain cached across roots. `Done` includes reference cleanup, and `Take` returns undrained results. Cancellation discards undelivered values and drains retained references. A single native call cannot be interrupted by the budget.

`FindPreparationSequences` examines each ability's straight `OnStart` list in an `AbilitySources` snapshot. It pairs a continuation with the nearest preceding mapping for the same control slot and retains intervening animation declarations, flags, targets, child indices and unread fields. Consumers handle conditional selection and visual playback.

## Phase visual data

`ReadVisualConfigSources(character_config)` resolves global and character visual rows against the base character phase. `ReadPhaseVisualConfigSources(character_config, source, types)` selects a `CharacterPhaseAnimConfig` from an explicit ability, global modifier or global template definition. `PhaseVisualSource` contains the root kind, definition name and declared field path. Paths support reference fields, array/list indices and string dictionary keys; local definitions are reached through their owner.

The selected phase replaces the base phase as a whole. A null final phase falls back to the base; an unread path reports an error. Results contain copied standby, revive, reaction and visual-state data. Global rows resolve their category references against the same selected phase. Consumers retain the separate global expression source.

Both readers prepare private row copies through the official visual-table helper. Cache the copied results outside per-frame playback; applying them is a separate operation.

`IsDeathVisualBehavior(row)` classifies a prepared row by its official animation category. Default and character hit inputs expose `die`, `die_hit`, `die_fly` and `die_limbo` names; phase inputs carry their corresponding overrides.

## Shared BGM synchronization

`PresentationContext::CreateActorBgmSync(entity, world)` binds an owned model, Art, Animator and controller. `Start(config)` forwards the official BGM state, BPM, chapters and phases to that actor's synchronization helper. Submit after the body animation starts. Synchronization uses the shared BGM clock; music playback remains with the game audio service.

`Accepted` confirms command return. `Observe(token)` reports whether the request's helper/config pair is active and whether its input references have drained. Retain any token returned by a failed start for cleanup. `Stop(token)` closes the matching request and preserves a replacement config. Call `Close()` until complete before releasing the client. Restoring an overwritten synchronization requires a new explicit request.

Keep the authored chapter and phase order. An absent `double_speed_phases` value maps to a null official x2 array; an explicit empty vector maps to an empty array.
