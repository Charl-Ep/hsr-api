# Presentation lifecycle

Engine operations and cleanup run on the owning official frame unless marked as preparation work.

## Context

Call `PresentationContext::Create(metadata, capabilities, policy, trace)` from `RuntimeClient::Prepare`. Keep the metadata/object providers and callback targets alive through all derived operations.

`PrepareSkin` returns shared resource preparation. `MapResource` returns the selected path or null for an officially suppressed resource; mapping failures throw. `Entities(skin)` and `Partners(skin)` bind it during actor creation. The context's animation policy applies private resource edits to those actors. Consumers select the prepared event-file lists and models; existing action/configuration identities remain separate.

Retire all bound actors before closing the context. Keep shared skins open while actors use them. `SelectSound` selects animation-event candidates; ordinary sound requests submit their supplied event name unchanged.

| Context API | Result |
|---|---|
| `Resources()` | Borrowed mesh, material, hierarchy and configuration call services |
| `Models()` | Shared model/renderer/Art provider with independent resource-owner factories |
| `Npcs()`, `NpcInputs()`, `NpcEvents()` | Creation, private input and event-supplement factories |
| `Entities()`, `EntityEvents()` | Map-NPC or independent story-actor creation and event supplements |
| `ReadSkins()`, `PrepareSkin(id)` | Skin catalog and shared official resource preparation |
| `Entities(skin)`, `Partners(skin)` | Bind skin resources to exact actor birth generations |
| `Partners()` | Auxiliary-actor creation factory for an owned summoner |
| `PartnerControls(partner, summoner, world)` | Member naming, attachment, animation and visibility-follow commands |
| `Animations()` | Per-entity animation session factory |
| `CreateActorTiming(entity, world)` | Actor clocks, official layer blending and animation lock speed |
| `CreateActorBgmSync(entity, world)` | Owned model synchronization with the shared official BGM |
| `CreateActorModelPresentation(entity, world)` | Model-bound face-material transitions, scale factors and plain scale following |
| `CreateModelScaleProgram()` | Composite model-scale operations on explicit owned targets |
| `CreateActorRendering(entity, world)` | Model-bound renderer masks, outlines and visibility keys |
| `Baselines()`, `ModelTransitions()` | Model capture and transition factories |
| `Controls()`, `Player()` | Control-session factory and current-player resolver |
| `EffectServices()`, `Outfits()` | Optional effect and outfit services |
| `CreateEffectEntityFollow()` | Request-scoped entity-follow plugin adapter |
| `CreateEffectLayouts()` | Layout operations on existing owned effect receipts |
| `StartSound(entity, world, event)` | Audio request bound to this client's current model Art |
| `CreateEffectObserver(before, after)` | Observer for official effect submissions from this client's entities |
| `CreateEventConstructionObserver(before, tasks, after)` | Final animation-event inputs and task-list construction for this client's entities |
| `CreateSequenceObserver(observe)` | Execution scopes for explicitly tracked event sequences |
| `CreateRoots()` | Independent GC-root collection |
| `ReadMap()`, `ReadPlayerPlacement()` | Frame-local observations |
| `ReadAnimator()`, `NpcMotionComponent()` | Current borrowed model/controller or FreeStyle component |
| `ReadEventReceiver(entity, world)` | Current event consumer, or null when unavailable |
| `CreateCatalog()`, `CreateMouseMode()` | Catalog reader and mouse-mode lease |
| `BindingErrors()`, `IsolationStatus()` | Preparation diagnostics and isolation state |
| `SetCameraPresentations(enabled)` | Show or hide this client's UI and camera-position-following effects on their next visibility evaluation |

Keep resource providers alive while their references are in use. Factories retain native context state beyond the wrapper's lifetime; each resource owner has an independent lifetime. Retain borrowed game objects explicitly.

Activate the context on the official frame before creating owned entities. `CreateAnimationPolicy` restricts camera noise and external head-look targets, and excludes the qualified shared input-lock and world-time tasks from owned animation events. Global intensity tasks follow `allow_timeline_environment`. Official entities keep their original inputs. Optional services may be unavailable independently.

Task ancestry is resolved across namespaces. Readable branches receive policy edits. Unreadable branches retain their original references and report `uninspected`; copy or write failure also preserves the original inputs. Treat these diagnostics as incomplete isolation.

Contexts share process-resident hooks and ownership records. Each client has its own policy and diagnostic sink; records are keyed by world, entity and generation. Retired clients' pending effects retain the policy needed to finish.

Camera presentation visibility preserves effect lifetime and the game's own visibility constraints. Its scope is UI and camera-position-following effects; actor attachments, LocalPlayer following and rotation-only billboards retain their usual visibility. Unresolved sources retain their original visibility and report diagnostics. Camera control and environment permissions are separate settings.

Drain entities, effects and registrations before `PresentationContext::Close`. Continue explicit cleanup while it returns false, then release the C++ owner.

## Character outfit

`Outfits()` creates a single-use `CharacterOutfit` owner. Call `Prepare(owned_entity, avatar_id)` after the entity loads, on its owning world frame. It selects the Avatar's declared path and gender using the current account outfit, then applies an entity-local override. Account equipment and skin selection remain unchanged.

`Status().gender` and `base_type` report raw official values. Outfit selection follows the current account; this API has no explicit skin or decoration selector.

Call `Close(true)` before disposing a live owner to restore its captured override. If applying the override required a configuration refresh, successful restoration also refreshes the parts. Once the entity is disposed, `Close(false)` only releases managed roots. Keep servicing closure until it completes; destruction makes no engine calls.

## Sound requests

`StartSound` resolves AudioManager on demand and posts an event on the owned actor's current Art. Keep the returned `OwnedSoundRequest` and call `Service()` on the owner frame. `Cancel()` is data-only; subsequent servicing submits the stop. An entity, world or Art change cancels the request.

`accepted` means the engine returned a nonzero logical ID; `started` and `ended` come from separate callbacks. `closed` means local roots and callback dispatch are released. Cancellation can close without an end callback, so observe `ended` separately. A bank-waiting request absent from both the queue and playing cache reports `dropped` and an error.

Cancellation selects queued and playing records by completion delegate and emitter. Queued entries release bank usage and their list node; active entries stop their actual playing IDs. Other records sharing the logical ID, music synchronization and event cooldowns remain unchanged. `SoundAccess::Stop` exposes the separate logical-ID group operation.

The low-level `SoundAccess`, `SoundCallbacks`, `SoundTracking` and `OwnedSoundRequest` APIs require caller-selected actors, listeners and retained inputs. Tracking errors are distinct from an empty queue; unknown cancellation outcomes are not retried.

## NPC creation and events

1. Prepare private inputs with `context.NpcInputs()(npc_id, name, position, rotation)`.
2. Create a single-use session with `context.Npcs()()` and submit `NpcCreateInput`.
3. Advance `Service` on matching world frames.
4. `Remove` requests retirement. Keep servicing until `Status().closed`.

Keep `MapNpcInputs` through the creation call, then drain its `Release` independently.

`context.NpcEvents()(npc, world)` creates a supplement that registers requested event files missing from the NPC baseline. `SetEnabled(false)` retires registrations while preserving sources; `RequestClose` also releases sources and is terminal.

Service the supplement before NPC retirement. The NPC session must outlive it. `InputsDetached` means no live registrations remain; `Quiescent` also waits for retired registrations; `Status().closed` includes source cleanup.

## Independent actors

`context.Entities()()` creates a single-use `EntityCreationSession`. Pass `StoryCreateInput` with the current world and manager, a unique name, a model prefab path and its adventure configuration path. The session retains its inputs and owns only entities reported by its creation scope. `Remove` and `Service` use the same disposal lifecycle as map NPCs.

Story creation uses official asynchronous model loading. Factory return establishes entity ownership; `Service` observes model readiness and retirement. Keep the session alive through both, including removal requested during loading.

`context.EntityEvents()(session, world)` supplements the loaded model's events. Story actors use the performance-event list as their factory baseline. Keep the supplement alive and service it before retiring the entity.

`TrackedEntities().BeforeDispose(token, callback)` registers a one-shot notification at the existing entity-disposal entry, before components are destroyed. Register and cancel on the owning engine thread; disconnect subscriptions before releasing callback targets. The callback must not dispose that entity. Exceptions are isolated and do not suppress the original call or other subscribers. Wrong-thread notifications are reported and not invoked.

`IsRetiring(token)` becomes true at this boundary, including when the original disposal later fails. `IsDisposed(token)` becomes true only after normal return. A stale or retiring generation cannot accept a new disposal subscription. `Forget` removes remaining subscriptions for that generation.

## Partner actors

`context.Partners()()` creates an `EntityCreationSession` for `PartnerCreateInput`: world, entity manager, summoner token, model prefab and character configuration. The summoner must be a live generation owned by the same context in that world. The session calls `EntityManagerExtension.CreatePartner` and retains inputs and birth receipts, including when the factory throws after creating an entity.

Continue `Service` through model loading and retirement until `Status().closed`. `Remove` retires the child. A summoner that ends also retires its remaining owned children; children already disposed by the engine are left intact.

Creation preserves the engine factory's notifications. Configure member names, attachment, animation synchronization and visibility through `PartnerControls`.

`PartnerControls` uses the created actor's official components. `SetMemberName` assigns its group name. `Attach` aligns two named model attach points and disables smooth following. `FollowAnimation` selects a synchronization target; an empty token clears it. `FollowVisibility` forwards the visibility relation; `None` or an empty target clears the relation. The game advances animation and attachment transforms.

`PlayIdle` submits a named animation to the partner's initialized official Idle logic state, with normalized start, transition and end inputs. It preserves the native entity-scoped animation notification. Observe animation completion separately.

`SetVisibility` adds or removes a keyed hide request through `CharacterVisibleComponent`. Its options control collider, trigger and audio muting while hidden. Setting `visible=true` removes that key; other active hide requests still apply. Use distinct keys for independent visual owners. Closing the control session releases references, not visibility requests; remove the key or retire the actor explicitly.

Commands require current entities from the same client and world, with the partner belonging to the specified summoner. Results preserve individual native-call outcomes, including partial failures. `Close` releases adapter references. Clear relations explicitly and retire the actor through its creation session. The Partner factory initializes its own configured animation events.

Creation receipts cover both `CreateEmptyEntity` and `_CreateNewEntityImpl`, including synchronous explicit-ID auxiliary entities. Nested returns of the same live object produce one birth. Attribution is limited to the active creation scope.

The context's Partner factory removes each owned group's exact `LevelCharacterDie` callback before disposal, including disposal inside the factory call. Other listeners remain registered. Failures are recorded in `Status().errors` while official disposal continues. This adapter is provided by the context's factory.

## Event construction observations

Prepare `CreateEventConstructionObserver` before model creation and activate it on the owner thread. It shares the construction hook with `AnimationEventPolicy`; `before` receives the final inputs after all active policies. Baseline and inline events may have null registration and file references.

`tasks` receives a core task-list factory's context, configuration collection, parent and returned executor array. Match executors to configurations by their receipts: disabled or failed tasks can be omitted. `after` reports normal construction return; playback is observed separately.

A nested factory can receive a different context. Use the enclosing construction for call provenance and each child's own receipt for ownership.

References are retained for their callback scope only. Retain anything needed afterwards. `Stop` detaches observations without removing game events; call it before releasing callback targets.

`CreateSequenceObserver` connects definitions to execution. Activate it on the owner thread and pass selected `tasks` receipts to `Track(source, tasks)`. Definitions use weak handles. `Collect` releases collected definitions; retirement emits `DefinitionRetired` with the former execution identity. Consumers use that event to release producer associations. Its pointers are opaque identity tokens. `Stop` detaches callbacks; `Close` also releases handles on the owning thread.

Each observed `Begin` receives a new execution serial. Persistent `Tick` retains that serial, while immediate sequences execute within `Begin`. Notifications include the operation, original return status, observed task state and `continues`. Reset, disposal, terminal return or exception ends the execution. Coverage starts at an observed `Begin` for a tracked definition and qualified entity generation.

## Model capture and transitions

`context.Baselines()(entity, world, profile)` captures the original model/event inputs and prepares a `ModelProfileRequest`. Keep overlays disabled during capture. Advance `Service`; when ready, transfer the request with `TakeRequest` and close the capture separately. Cancellation stops preparation without destroying the entity.

Pass the request to `context.ModelTransitions()(entity, world, request)`. A null request supports the existing factory model. Use `Select` to change the destination/controller, then advance `Service`.

The transaction cancels pending loading, detaches its controller, waits for consumer overlays, loads/adopts the model, applies the controller and binds event inputs. `ModelTransitionConsumer` supplies synchronous detach/adopt and optional initial-output callbacks. Initial output is observed on a later official frame before readiness.

A new `Select` supersedes a pending plan. `Cancel` is terminal. Replacing an action within the same model plan does not require restarting the transaction.

Runtime-bound profile requests submit alternate models asynchronously. They wait for the official operation to stop, then verify the resolved path, model completion and Animator readiness before adopting events. A completed request with the wrong path reports failure. Cancellation withdraws the resource request before releasing input roots, including after entity disposal. Baseline configuration restoration uses the official restore entry.

Call `Close` before entity retirement to detach overrides; call it again after retirement to release retained event inputs. Reentrant close waits for the active call to unwind. Uncertain reference decrements are not repeated.

## Placement and gaze

`ActorControlSession::Place` queues absolute poses or relative world-space offsets. Relative rotations multiply on the left. The write occurs after that actor's official movement flush. `PlannedPlacement` combines observed and pending poses without a game call.

`Look` selects automatic behavior, disabled behavior, an entity head or a world point. Targets must belong to the same world. World-point targets use a private reusable Unity object; pose solving stays with the official LookAt component.

Before replacing a model, call `Suspend` and service with `presentation_ready=false`. `Waiting` means official gaze fade-out is active; `Returned` permits model replacement. `Cancel` stops new work. `Close` restores policy, releases the point after native destruction and drains roots.

When a carrier has no adventure MonoAvatar, the session can use an existing LookAtIK matching the model's Head attachment. Manual gaze temporarily enables a disabled matching Behaviour. Restoration waits for fade-out before restoring its original enabled state.

## Model resources

`ModelServices` exposes entity/model/renderer/Art calls. Its root, asset-lease and native-object factories return separate owners. `ComponentType` returns an unrooted managed type; retain it before keeping it across calls.

`LookSupport` can add an official LookAt component to a private inactive model. It loads an independent NPC prefab, serializes the component and remaps local transform references by relative hierarchy path. Ambiguous references fail supplementation. `SettleFailure` drains partial additions; a successful addition belongs to the model's lifetime, and `Close` observes its destruction.

## Effects

`context.EffectServices()` returns an optional `PresentationEffects` provider. `CreateRequest` constructs an `OwnedEffectRequest`; `RegisterExit` prepares exit-event configuration.

`MapPath` returns an unrooted caster-mapped path. Retain it before another managed call. To submit an already mapped path, pass `EffectSubmissionOptions::path_mode = EffectPathMode::Resolved` to `StartEntity` or `StartSpatial`; the default maps through the caster. `enable_replay` controls the new request's replay flag. `Replay` applies official replay behavior to its retained receipt.

An activated context tracks native effect submissions attributed to its created entities. After an entity generation ends, the shared runtime retires its remaining receipts through the official effect manager, outside the producing call stack, and observes release or pool reuse. This includes late submissions. Consumers also clean up effects at action and model-transition boundaries.

`Start` retains the request's parameters, original manager and completion bridge. Callbacks record signals; the owner frame performs removal. After cancellation, advance `Service` until `Status().closed`. An unmatched removal is retried after a new successful-load callback. A reused parameter-pool generation ends writes through the old request.

`StartEntity(caster, target, path, options)` keeps the creator and recipient distinct. It maps the path through the caster, retains both entities, and uses the official parameter pool and asynchronous manager. Options specify the engine name, position/rotation offsets, optional scale, attachment, fade-in and immediate simulation. Inputs are copied before engine calls; cancellation uses the same request lifecycle as `Start`.

`attach_point` selects an initial named attachment such as `Muzzle`. It is independent of `attach_to_target`, which controls continued parenting. The loader resolves the attachment before initializing the effect; an empty name leaves the prefab's default attachment policy in place.

Entity and spatial options accept `resident` to set `MonoEffectFlag.Resident`. The request owner remains responsible for removal. The SDK supplies an immutable managed flag array through the official field setter.

This entry accepts resolved entity-relative inputs. Callers select targets, evaluate expressions and mutex rules, and arrange any loading visibility, Timelines or post-load operations. Programs own reuse and logical effect names. Engine `unique_name` is optional; ownership uses the request receipt.

`StartSpatial(caster, path, options)` submits an effect at a world position and quaternion rotation. The caster supplies resource mapping and ownership. The SDK normalizes a nonzero quaternion and sets the loader's initial pose and final position anchor before submission. Scale, engine name, fade-in and immediate simulation are optional. Effect animation may move it after initialization. Cancellation and late completion use the same owned request lifecycle.

For an independently attributed official submission, `TrackSubmitted(manager, reference)` retains the existing receipt and callbacks. It returns false if retention fails; ownership then remains with the caller. Service tracked receipts after the original call returns. A newly observed loaded instance permits another cancellation attempt. Observe completion through the retained receipt; callback completion counters remain unchanged in this mode.

Create an effect observer during preparation, then call `ActivateOnCurrentThread` on the owner frame. `before` receives the retained manager, request generation and entity provenance. Synchronous and asynchronous forwarding of the same request is reported once. `after` reports normal return or exception; loading has a separate lifecycle. Stop the observer on the owner thread before releasing callback targets.

`EffectSubmission::trigger` identifies a qualified `TriggerEffect.OnTaskBegin` invocation and its task, configuration and context. Direct or unattributed calls leave it empty. Its serial identifies that invocation. Retain needed references during the callbacks and service cancellation after the producing engine stack returns.

`EffectSubmission::sequence` links the trigger to an enclosing tracked execution from the same entity generation, including its construction and definition serials. Attribution covers tracked sequence calls; direct submissions and submission callbacks have independent provenance. Observe sequence completion and asynchronous effect completion separately.

`SetProgress(reference, value, seconds)` controls the loaded effect's progress plugin. `SetAnimatorState(reference, hash, duration, restart)` controls its first Animator on layer zero; duration is normalized transition time. Keep the request alive during either call. Results distinguish applied calls, pending loads, ended requests, unsupported components and failures. Control ends at parameter release or reuse; it remains available during a fade.

The `SetAnimatorState(reference, std::string, duration, restart)` overload resolves the state with Unity's `Animator.StringToHash`, caches the result, and rechecks the request after managed input preparation. State-name resolution stays within the SDK.

`SetAnimatorTrigger(reference, name, reset)` sets or resets a nonempty trigger on all child Animators, including inactive children. A pending effect returns `NotLoaded`.

`SetAnimatorParameter(reference, parameter)` accepts a name and a typed `float`, `int`, `bool` or `EffectParameterTrigger`. `FirstActive` sets the first Animator on the effect or its active children. `FirstMatchingParameter` additionally requires a matching parameter name and type. `NoMatch` means no setter ran; pending loads return `NotLoaded`. Both modes take resolved values and retain the exact request identity.

`OwnedEffectRequest::Cancel(EffectRemovalOptions)` accepts `fade`, `immediate_fade` and `unbind`. `Service` forwards them with the retained request ID. Keep servicing until `closed` confirms cleanup. Removal requires a nonzero ID: the engine treats zero as an unrestricted filter.

See [animation inputs](animation.md) and the [C++ ownership contract](cpp-interface.md).

### Damage-hit effects

`OwnedEffectRequest::StartHit(caster, target, path, placement)` creates an entity-relative hit effect. `HitEffectPlacement` carries its local offset, world alignment direction, delay and prefab-attachment policy. The load callback applies the official hit-alignment operation to that request. `UpdateHit(placement)` refreshes a loaded request's attachment, replays its behavior and reapplies alignment; pending loads return `NotLoaded`.

`ActorAnimationSession::ReadHitEffectPlacement` derives these spatial inputs from the receiver's current shape, attachment and transform. Offset overrides and optional world-scale adjustment are explicit inputs. Resource selection, modifier precedence and duplicate-request selection belong to the caller. Cancellation and servicing use the same `OwnedEffectRequest` lifetime as other effects.

`PresentationEffects::ReadHitRules` reads the current public effect configuration. `HitMutex` runs the official global/entity checks and selects replay candidates from supplied owned receipts. `OwnedEffectRequest::ReplayHit` revives and replays a receipt at its existing placement. `MatchesHitPath` compares the loaded effect's name with the current caster-mapped path. Request ownership remains with the caller.

## Entity motion driven by effects

`context.CreateEffectEntityFollow()` creates an engine-thread adapter for `MonoEffectPluginEntityFollowEffect`. `Read(reference)` reports its current target identities. `Set(reference, expected_targets, desired_targets)` changes the binding only when the observed list matches the expected list. An empty desired list invokes the official unbind operation; an unchanged list performs no write or allocation.

Targets are explicit managed `GameEntity` references. The caller retains requests and targets, arbitrates competing motion owners, and clears the binding before retiring targets. The plugin performs animation-driven movement and removes accumulated translation through the game's updates. Pending effects must finish loading before binding.

`ReadTarget(entity)` reads the existing movement component's attached-motion flag; track its owner separately. The optional `Set` predicate rechecks caller-owned lifetimes after managed input preparation, immediately before the native write. The predicate must be read-only.

Clear the binding, then call `Close()` to release cached type references. A failed native call may have applied part of the change; inspect the actual list with `Read` before cleanup.

## Common Timelines

`context.CreateTimeline(entity, world)` creates an `OwnedTimeline` for the client's entity generation. `Start(path, wrap_mode, start_time)` uses the official global Timeline service with zero position/rotation and default asset/actor binding. Wrap modes are the engine's integer values 0–2. The API covers playback and registry ownership; task-level completion and volume notifications remain the caller's responsibility.

`Cancel` records intent; `Service` stops the captured record and observes its registry borrow. Keep servicing until `Status().drained`. Stopped Directors remain owned until pool return, removal or reuse. An interrupted start retains attributed borrows and reports incomplete attribution. The game advances the Director's clock.

## Actor audio routing

`SetActorAudio(entity, world, events, rate)` registers exact event names with their Normal and Double audio-node IDs for one owned actor generation. Rate is 1 or 2. An empty event list removes the registration without preparing audio interfaces.

One shared PostEvent adapter selects the node only for that actor's current event Art and the official AudioManager callback. The original emitter, flags, callback and cookie are preserved. AudioManager retains its bank preparation, logical IDs, cache, completion and StopEventID behavior. Other actors and unmapped requests use the original PostEvent call.

`StartSound` uses the same official submission path and returns an owned sound receipt. `SetActorAudioRate` selects the branch for subsequent sounds; existing playback and global Wwise State remain unchanged. Current Art is refreshed during preparation, official frames and event construction. Routing requires a live actor and its current Art.

`ClearActorAudio` withdraws one registration. Drain sound receipts before releasing them and call context `Collect` during retirement. External-source playback, Event-based seeking and global battle speed retain their original paths.

Mapped playback bypasses the Wwise Event's Action graph and enqueues the selected audio node directly. Register only events qualified as one unmodified Play action into the intended speed branch. Event delays, random Action properties, multiple actions and external sources are not reproduced. The original callback object is preserved, but Event IDs and Event-specific marker/duration payload semantics are not guaranteed equivalent. End-of-event and ordinary playing-ID stop are supported by the native sequence path.

Audio-node IDs belong to the installed game resources. Verify the event-to-node catalog when those resources change and register exact Event-to-node mappings. Unregistered events retain ordinary Event playback.

Emitter lookup uses an exact index and verifies the current event-component identity before reading Art. Component replacement ends the old registration's eligibility.

An unknown native-write outcome retains its receipt and references for diagnosis. Context closure can remain incomplete; keep the SDK loaded for the process lifetime.

`ReadVoiceEvent(entity, world, avatarId, cue)` reads the Avatar row's voice tag and official event template, applies the selected skin sound candidate, and returns an existing event name. Supported cues are `LightHit` and `UltraReady`; an empty result means no configured or installed event. Pass the selected Avatar ID, then submit a returned event through `StartSound`.

`ReadReadyVoiceCue(skillType)` classifies the original skill-type value using the current official enum. The ultimate type returns `UltraReady`; ordinary ready states return no cue. These voice queries select data; playback is explicit.

`ReadActorEventFiles(entity, world)` copies the current consumer's adopted event-file paths, including later changes. An unavailable consumer is an error; a valid empty array is returned as empty. Inline definitions use the separate event-definition reader.

## Attachment visibility

`CreateAttachmentVisibility(entity, world)` controls named attachments on the client's current owned model. `Set(scope, point, visible, delay)` forwards to the official visibility component. Delay is an integer frame count independent of battle time scale.

Creation captures the current actor/model identity on the owner frame. Actor input
roots are acquired at the first `Set`, after rechecking that identity. Close the
session even if preparation or a later write throws. `Close` during an operation
returns false; finish cleanup after that operation unwinds.

`Release(scope)` withdraws that scope. Overlapping scopes restore the next live value, then the original value. A different native value observed during withdrawal is preserved. `Close()` withdraws all scopes and releases references; call it before model replacement. Writes require the captured live entity generation and Art. Scale and parenting are separate operations.

## Attachment scale

`CreateAttachmentScale(entity, world)` applies nonnegative three-axis scales to named attachments on the client's current owned model. `Set(scope, point, scale)` uses the official continuous `AttachPointSet` driver; parent constraints remain unchanged.

Creation captures model identity; the first `Set` acquires actor inputs and the
driver type after revalidation. Preparation errors leave their partial roots with
the session for explicit `Close`. Close during an operation returns false and
defers restoration and release until that operation returns.

`Release(scope)` restores another live contribution or the captured native state, preserving newer native changes. `Close()` releases all contributions and retained references. Call it before model replacement. The service accepts explicit attachment names and local scale contributions.

## Event definition selection

During the construction observer's `before` callback, `context.ReadEventDefinitions(source)` reads final time-point, group-enter, group-exit and behavior task collections. Rows contain copied names and borrowed collection references; issues are reported per row. The result includes entries with task collections.

Match a row's `configurations` against the actual task-builder receipt in the same construction scope; disabled or unconstructed tasks may be absent from executor arrays. The reader inspects final in-memory inputs. Retain references explicitly when keeping them beyond the callback scope.

`context.ReadEffectTriggerPath(submission.trigger)` copies the attributed trigger's original `EffectPath` inside the submission callback. Use it as a source label. An absent trigger and an empty configured path produce distinct results.

## Model presentation

`CreateActorModelChange(entity, world, path)` creates a one-shot synchronous request on the client's owned model component. A null path uses official default-path resolution. `Service` calls `CanChangeModel`, then `ChangeModel`; `Ready` confirms a true return on that component. Check animation-consumer readiness separately. `Unchanged` has no success continuation. Cold resource loading can stall the call. Cancellation prevents submission; restoring a submitted change requires a separate model request. Service `Close` until input references drain.

`CreateActorVisualConfiguration(entity, world)` binds the client's current model. `Current()` checks that identity; `Current(component)` checks a captured component; `Current(input)` checks a request's component dependencies, including conditional event reset. Replacement returns false; lookup and adaptation errors throw. Each adaptation failure affects its dependent requests.

`Apply` sets visual overrides for radius, somato type, effect adaptations, graph-emotion path and animation-event files. It rebinds event inputs before refreshing existing effects. `reset` selects official defaults for this subset. An empty graph path selects the default configuration. Omitted fields remain unchanged; an explicit empty event array clears event files.

`EnableEmotion(enabled, transit_out, transit_in)` forwards the official emotion-component enable-source transition and blendshape enable operations. An actor without a supported emotion component returns false. The alternative Mono path is unsupported. Controller mode, graph construction and unrelated entity/UI state remain unchanged. Use explicit reset operations before `Close`, which releases the binding.

Actors without `CharacterDataComponent` report requested radius, somato, adaptation and graph overrides in `unsupported_fields`; component lookup failures report errors. Event-file rebinding works independently and updates the Data overlay when present. Emotion enable has separate dependencies.

Explicit reset `event_files` take precedence over the captured baseline. Supply the original configuration's list when restoring across model changes. Otherwise, reset restores the paths and excluded layers captured before the first event operation. Rebinding requires an initialized event consumer and verifies its adopted paths. Playing event-group instances require separate lifecycle management.

Low-level `ActorVisualConfigurationBinding.current` qualifies the actor, world and model. `current_component` qualifies the requested `Data`, `Events` or `Emotion` input. A provider that omits it must qualify its supplied components through `current`. Custom `ActorVisualConfigurationServices` can provide request-specific identity checks through `current_input`; omitted identity callbacks use `current`'s qualification.

`CreateActorModelPresentation(entity, world)` binds the owned entity generation, model and Art. `SetFace` forwards cheek, shy, shadow and duration to the official material transition. `SetScaleFactor` changes one named whole-model factor. `Follow` supports the plain, empty-group model-follow branch; an empty target stops it. Group propagation, attachment multipliers, outline following and LateUpdate mode are unsupported. Those branches throw `ModelPresentationUnsupported`; native write failures may be partial. Withdraw changes before `Close`, which releases references only.

`CreateModelScaleProgram()` provides the official `ScaleCharacterModel` operation. `Set(scope, targets, input)` takes explicit owned targets, base scale, outline scale, an ordered collider-height multiplier table and an optional shared minimum. It prepares every target before applying the per-target helper, including model notifications and outfit refresh.

An unsupported input produces `Unsupported` without an accepted model write. A failed batch may contain applied targets; keep its program and call `Release(scope)` until it drains. Release restores overlapping contributions or the captured baseline and preserves separately changed native factor/outline values. `ReleaseParticipant` withdraws contributions before that target's model replacement. Ended or replaced models are never written. `Close` withdraws all remaining scopes and releases references.
