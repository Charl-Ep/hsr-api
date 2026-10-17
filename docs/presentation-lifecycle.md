# Presentation lifecycle

Engine operations and cleanup run on the owning official frame unless marked as preparation work.

## Context

Call `PresentationContext::Create(metadata, capabilities, policy, trace)` from `RuntimeClient::Prepare`. Keep the metadata/object providers and callback targets alive through all derived operations.

| Context API | Result |
|---|---|
| `Resources()` | Borrowed mesh, material, hierarchy and configuration call services |
| `Models()` | Shared model/renderer/Art provider with independent resource-owner factories |
| `Npcs()`, `NpcInputs()`, `NpcEvents()` | Creation, private input and event-supplement factories |
| `Entities()`, `EntityEvents()` | Map-NPC or independent story-actor creation and event supplements |
| `Partners()` | Auxiliary-actor creation factory for an owned summoner |
| `PartnerControls(partner, summoner, world)` | Member naming, attachment, animation and visibility-follow commands |
| `Animations()` | Per-entity animation session factory |
| `Baselines()`, `ModelTransitions()` | Model capture and transition factories |
| `Controls()`, `Player()` | Control-session factory and current-player resolver |
| `EffectServices()`, `Outfits()` | Optional effect and outfit services |
| `CreateEffectEntityFollow()` | Request-scoped entity-follow plugin adapter |
| `CreateEffectObserver(before, after)` | Observer for official effect submissions from this client's entities |
| `CreateEventConstructionObserver(before, tasks, after)` | Final animation-event inputs and task-list construction for this client's entities |
| `CreateSequenceObserver(observe)` | Execution scopes for explicitly tracked event sequences |
| `CreateRoots()` | Independent GC-root collection |
| `ReadMap()`, `ReadPlayerPlacement()` | Frame-local observations |
| `ReadAnimator()`, `NpcMotionComponent()` | Current borrowed model/controller or FreeStyle component |
| `ReadEventReceiver(entity, world)` | Current event consumer, or null when unavailable |
| `CreateCatalog()`, `CreateMouseMode()` | Catalog reader and mouse-mode lease |
| `BindingErrors()`, `IsolationStatus()` | Preparation diagnostics and isolation state |

Resource references require their provider to remain alive. Factories retain native context state beyond the wrapper's lifetime. Sharing a provider does not merge resource owners or retain borrowed game objects.

Activate the context on the official frame before creating owned entities. `CreateAnimationPolicy` applies task policy on that frame; its default restricts camera noise and confines head-look event targets to the owned entity. Optional services may be unavailable independently.

Contexts share process-resident hooks and ownership records. Each client has its own policy and diagnostic sink; records are keyed by world, entity and generation. Retired clients' pending effects retain the policy needed to finish.

Drain entities, effects and registrations before `Context::Close`. Continue explicit cleanup while it returns false. Destruction performs no substitute game cleanup.

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

Story creation requests the official asynchronous model-loading path. Factory return establishes entity ownership, not model readiness. Keep the session alive and continue servicing it while the model loads, including when removal is requested before loading finishes.

`context.EntityEvents()(session, world)` supplements the loaded model's events. Story actors use the performance-event list as their factory baseline. Keep the supplement alive and service it before retiring the entity.

`TrackedEntities().BeforeDispose(token, callback)` registers a one-shot notification at the existing entity-disposal entry, before components are destroyed. Register and cancel on the owning engine thread; disconnect subscriptions before releasing callback targets. The callback must not dispose that entity. Exceptions are isolated and do not suppress the original call or other subscribers. Wrong-thread notifications are reported and not invoked.

`IsRetiring(token)` becomes true at this boundary, including when the original disposal later fails. `IsDisposed(token)` becomes true only after normal return. A stale or retiring generation cannot accept a new disposal subscription. `Forget` removes remaining subscriptions for that generation.

## Partner actors

`context.Partners()()` creates an `EntityCreationSession` for `PartnerCreateInput`: world, entity manager, summoner token, model prefab and character configuration. The summoner must be a live generation owned by the same context in that world. The session calls `EntityManagerExtension.CreatePartner` and retains inputs and birth receipts, including when the factory throws after creating an entity.

Factory return does not imply model readiness. Continue `Service` while loading and retiring. A summoner that ends causes the session to request retirement of any remaining owned child; children already disposed by the engine are not removed twice. `Remove` retires the child, not its summoner. Keep servicing until `Status().closed`.

This entry does not execute a summon task or configure member names, attachment, animation synchronization or visibility. The engine factory's notifications remain intact. Configure subsequent visual behavior separately.

`PartnerControls` uses the created actor's official components. `SetMemberName` assigns its group name. `Attach` aligns two named model attach points and disables smooth following. `FollowAnimation` selects a synchronization target; an empty token clears it. `FollowVisibility` forwards the visibility relation; `None` or an empty target clears the relation. These operations do not advance animation or recalculate attachment transforms.

Commands require current entities from the same client and world, with the partner belonging to the specified summoner. Results preserve individual native-call outcomes, including partial failures. `Close` releases adapter references; it does not destroy the actor or reverse its relations. Retire the actor through its creation session. The factory initializes its configured animation events; the NPC event supplement is not a Partner baseline reader.

Creation receipts cover both `CreateEmptyEntity` and `_CreateNewEntityImpl`, including synchronous explicit-ID auxiliary entities. Nested returns of the same live object produce one birth. These scopes do not attribute unrelated asynchronous creations.

## Event construction observations

Prepare `CreateEventConstructionObserver` before model creation and activate it on the owner thread. It shares the construction hook with `AnimationEventPolicy`; `before` receives the final inputs after all active policies. Baseline and inline events may have null registration and file references.

`tasks` receives a core task-list factory's context, configuration collection, parent and returned executor array. Disabled or failed tasks can be omitted, so their indices are not a configuration-to-executor mapping. `after` reports whether event construction returned normally. Neither result describes a playback or proves visual output.

A nested factory can receive a different context. Its enclosing construction identifies the call source, not ownership of every returned child.

References are retained for their callback scope only. Retain anything needed afterwards. `Stop` detaches observations without removing game events; call it before releasing callback targets.

`CreateSequenceObserver` connects those definitions to execution. Activate it on the owner thread and pass selected `tasks` receipts to `Track(source, tasks)`. Definitions use weak handles; they do not retain game sequences. `Collect` releases collected definitions. Retirement emits `DefinitionRetired` with the former execution identity, allowing consumers to release producer associations even without a game disposal callback. Its pointers are identity tokens only, not readable objects. `Stop` detaches callbacks; `Close` also releases handles on the owning thread.

Each observed `Begin` receives a new execution serial. Persistent `Tick` retains that serial, while immediate sequences execute within `Begin`. Notifications include the operation, original return status, observed task state and `continues`. Reset, disposal, terminal return or exception ends the execution. Missing starts are not reconstructed from a later tick. Only tracked definitions and qualified entity generations propagate provenance; direct helper calls outside these entries are not covered.

## Model capture and transitions

`context.Baselines()(entity, world, profile)` captures the original model/event inputs and prepares a `ModelProfileRequest`. Keep overlays disabled during capture. Advance `Service`; when ready, transfer the request with `TakeRequest` and close the capture separately. Cancellation stops preparation without destroying the entity.

Pass the request to `context.ModelTransitions()(entity, world, request)`. A null request supports the existing factory model. Use `Select` to change the destination/controller, then advance `Service`.

The transaction cancels pending loading, detaches its controller, waits for consumer overlays, loads/adopts the model, applies the controller and binds event inputs. `ModelTransitionConsumer` supplies synchronous detach/adopt and optional initial-output callbacks. Initial output is observed on a later official frame before readiness.

A new `Select` supersedes a pending plan. `Cancel` is terminal. Replacing an action within the same model plan does not require restarting the transaction.

Runtime-bound profile requests submit alternate models asynchronously. They wait for the official operation to stop, then verify the resolved path, model completion and Animator readiness before adopting events. A completed request that leaves the wrong path is a failure. Cancellation withdraws the resource request before input roots are released, including after entity disposal. Restoring the baseline entity configuration still uses the existing restore entry.

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

`Start` retains the request's parameters, original manager and completion bridge. Callbacks record signals; the owner frame performs removal. After cancellation, advance `Service` until `Status().closed`. An unmatched removal is retried after a new successful-load callback. A reused parameter-pool generation ends writes through the old request.

`StartEntity(caster, target, path, options)` keeps the creator and recipient distinct. It maps the path through the caster, retains both entities, and uses the official parameter pool and asynchronous manager. Options specify the engine name, position/rotation offsets, optional scale, attachment, fade-in and immediate simulation. Inputs are copied before engine calls; cancellation uses the same request lifecycle as `Start`.

`attach_point` selects an initial named attachment such as `Muzzle`. It is independent of `attach_to_target`, which controls continued parenting. The loader resolves the attachment before initializing the effect; an empty name leaves the prefab's default attachment policy in place.

This entry accepts resolved entity-relative inputs. It does not evaluate target aliases or task expressions, consult global mutex rules, hide entities during loading, attach Timelines or run post-load subobject tasks. Programs own reuse and logical effect names. Engine `unique_name` is optional and is not an ownership boundary.

`StartSpatial(caster, path, options)` submits an effect at a fixed world position and quaternion rotation, without a target entity or attachment. The caster still supplies resource mapping and ownership. The SDK normalizes a nonzero quaternion and writes both the loader's initial pose and the final position anchor before submission. Scale, engine name, fade-in and immediate simulation are optional inputs. Effect animation may move it after initialization. Cancellation and late completion use the same owned request lifecycle.

For an independently attributed official submission, `TrackSubmitted(manager, reference)` retains the receipt without submitting again or replacing callbacks. It returns false if retention fails; ownership then remains with the caller. Service tracked receipts after the original call returns. A newly observed loaded instance permits another cancellation attempt. This mode does not report official callbacks through the request's completion counters.

Create an effect observer during preparation, then call its `ActivateOnCurrentThread` on the owner frame. `before` receives the retained manager, request generation and entity provenance before the official call. Synchronous and asynchronous forwarding of the same request is reported once. `after` reports normal return or exception, not loading success. Stop the observer on the owner thread before releasing its callback targets.

`EffectSubmission::trigger` identifies a qualified `TriggerEffect.OnTaskBegin` invocation and its task, configuration and context. It is empty for direct or unattributed calls; its serial is not an animation-state or event-group ID. Retain needed references during the callbacks. Preserve official callbacks and defer cancellation servicing until the producing engine stack has returned.

`EffectSubmission::sequence` links that trigger to an enclosing tracked sequence execution from the same entity generation. It includes the construction and definition serials. Untracked sequences, direct submissions and callbacks inside submission processing do not inherit an outer execution. Sequence completion does not imply that its asynchronous effects have finished.

`SetProgress(reference, value, seconds)` controls the loaded effect's progress plugin. `SetAnimatorState(reference, hash, duration, restart)` controls its first Animator on layer zero; duration is normalized transition time. Keep the request alive during either call. Neither operation loads a pending effect or searches by name. Results distinguish applied calls, pending loads, ended requests, unsupported components and failures. A fade request does not itself end control; parameter release or reuse does.

The `SetAnimatorState(reference, std::string, duration, restart)` overload resolves the state with Unity's `Animator.StringToHash`, caches the result, and rechecks the request after managed input preparation. State-name resolution stays within the SDK.

`SetAnimatorTrigger(reference, name, reset)` sets or resets a nonempty trigger on all child Animators, including inactive children. A pending effect returns `NotLoaded`.

`OwnedEffectRequest::Cancel(EffectRemovalOptions)` accepts `fade`, `immediate_fade` and `unbind`. `Service` forwards them to the official effect manager with the retained request ID. Keep servicing until `closed`; accepted removal is not completed destruction. ID zero is rejected by both removal paths because the engine treats it as an unrestricted filter.

See [animation inputs](animation.md) and the [C++ ownership contract](cpp-interface.md).

## Entity motion driven by effects

`context.CreateEffectEntityFollow()` creates an engine-thread adapter for `MonoEffectPluginEntityFollowEffect`. `Read(reference)` reports its current target identities. `Set(reference, expected_targets, desired_targets)` changes the binding only when the observed list matches the expected list. An empty desired list invokes the official unbind operation; an unchanged list performs no write or allocation.

Targets are explicit managed `GameEntity` references. The caller retains the request and target lifetimes, arbitrates competing motion owners, and clears the binding before retiring targets. The plugin performs animation-driven movement and removal of accumulated translation. The adapter does not load pending effects, select global targets, or advance animation.

`ReadTarget(entity)` reads the existing movement component's attached-motion flag. The flag does not identify its owner. The optional `Set` predicate rechecks caller-owned lifetimes after managed input preparation, immediately before the native write; it must not mutate state.

Call `Close()` to release the adapter's cached type references. It does not undo an active binding. A failed native call may have applied part of the change; use `Read` to inspect the actual list before deciding cleanup.

## Common Timelines

`context.CreateTimeline(entity, world)` creates an `OwnedTimeline` for that client's entity generation. `Start(path, wrap_mode, start_time)` uses the official global Timeline service with zero position/rotation and no explicit asset or actor-binding callback. Wrap modes are the engine's integer values 0–2. It does not execute a `PlayTimeline` task or its global completion/volume notifications.

`Cancel` records intent; `Service` stops the captured record and observes its registry borrow. Keep servicing until `Status().drained`. Stopped Directors remain owned until pool return, removal or reuse. Requests retain any attributed borrows from an interrupted start; incomplete attribution remains diagnostic, not successful cleanup. Service never updates a Director's animation clock.

## Event definition selection

During the construction observer's `before` callback, `context.ReadEventDefinitions(source)` reads the final time-point, group-enter, group-exit and behavior task collections. Rows contain copied names and borrowed collection references; issues are reported per row. Logic entries have no task collection and are not included.

Match a row's `configurations` against the actual task-builder receipt inside the same construction scope. Do not pair configuration and executor arrays by index: disabled or unconstructed tasks may be absent from the result. The reader neither loads another config file nor starts any task. Retain managed references explicitly if they must outlive the construction callback scope.

`context.ReadEffectTriggerPath(submission.trigger)` copies the attributed trigger's original `EffectPath` inside the submission callback. It is a source label, not an effect lookup. An absent trigger and an empty configured path produce distinct results.
