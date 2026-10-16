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
| `Animations()` | Per-entity animation session factory |
| `Baselines()`, `ModelTransitions()` | Model capture and transition factories |
| `Controls()`, `Player()` | Control-session factory and current-player resolver |
| `EffectServices()`, `Outfits()` | Optional effect and outfit services |
| `CreateEffectObserver(before, after)` | Observer for official effect submissions from this client's entities |
| `CreateRoots()` | Independent GC-root collection |
| `ReadMap()`, `ReadPlayerPlacement()` | Frame-local observations |
| `ReadAnimator()`, `NpcMotionComponent()` | Current borrowed model/controller or FreeStyle component |
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

For an independently attributed official submission, `TrackSubmitted(manager, reference)` retains the receipt without submitting again or replacing callbacks. It returns false if retention fails; ownership then remains with the caller. Service tracked receipts after the original call returns. A newly observed loaded instance permits another cancellation attempt. This mode does not report official callbacks through the request's completion counters.

Create an effect observer during preparation, then call its `ActivateOnCurrentThread` on the owner frame. `before` receives the retained manager, request generation and entity provenance before the official call. Synchronous and asynchronous forwarding of the same request is reported once. `after` reports normal return or exception, not loading success. Stop the observer on the owner thread before releasing its callback targets.

`EffectSubmission::trigger` identifies a qualified `TriggerEffect.OnTaskBegin` invocation and its task, configuration and context. It is empty for direct or unattributed calls; its serial is not an animation-state or event-group ID. Retain needed references during the callbacks. Preserve official callbacks and defer cancellation servicing until the producing engine stack has returned.

`SetProgress(reference, value, seconds)` controls the loaded effect's progress plugin. `SetAnimatorState(reference, hash, duration, restart)` controls its first Animator on layer zero; duration is normalized transition time. Keep the request alive during either call. Neither operation loads a pending effect or searches by name. Results distinguish applied calls, pending loads, ended requests, unsupported components and failures. A fade request does not itself end control; parameter release or reuse does.

See [animation inputs](animation.md) and the [C++ ownership contract](cpp-interface.md).
