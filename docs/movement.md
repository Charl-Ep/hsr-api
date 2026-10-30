# Animation movement

## Facing

`PresentationContext::FaceEntity` turns an owned actor toward an explicit world point. It uses the shortest horizontal angle, a clamped ratio and an angle offset. Positive duration uses the official Transform smoothing; nonpositive duration applies immediately. The return value reports submission, not the end of rotation. A later turn replaces the target. This operation does not move team members or change navigation policy.

## Animation-driven movement

`PresentationContext::StartMovement` submits one animation-driven movement on an owned entity. It captures the current position, model and controller, validates the named base-layer state and returns an `OwnedMovementRequest`. It does not play the animation.

For Story actors, the context adds the official animation-movement component on demand. Component registration, model callbacks and updates use the game's lifecycle. The context caches the closed generic factory rather than resolving it for each actor.

`AcquireMovementHost` returns an exclusive lease shared by the animation-movement and effect-follow services. The lease pauses ordinary Adventure root translation and the movement component's raw root callback. It leaves animation and IK callbacks running. `Ready` waits for previously submitted Adventure movement to drain; `waiting_for_host` reports this state on an animation request. The request samples its origin after that wait.

Close the lease after detaching its motion channels and before changing the model. Closing restores the original motion policy and removes only a component created by that lease. An existing native component is retained. Entity retirement ends access to its native objects. Neither the lease nor a request destructor calls the game.

Service the request on the engine thread. Cancel and drain it before a model handoff. The request also cancels at entity pre-disposal and detects unexpected model changes. Completion, interruption, replacement, cancellation and released references have separate status fields.

`AnimationMovementData` and `AnimationMovementAccess` are the lower-level data and component adapters.

## Data

`PresentationContext::StartPosition` applies a resolved world point through the official movement component. It returns the same serviced request type and uses the exclusive motion lease, but creates no animation graph or state-key dependency. Pending Adventure movement drains before submission. A successful call reports `completed`; cancellation before submission performs no positioning. The call updates the game's TransformComponent rather than a Unity Transform directly.

`ReadMovementGeometry` reads an entity's current `CharacterDataComponent` radius and local target offset. It returns no value when that component is absent and never creates one. `OfficialCatalog::ReadCharacterVisualRadius` reads the selected character configuration's base radius. These values are independent of collision bounds.

An `AnimationMovePlan` contains an origin, an Animator state key and ordered segments with resolved world-space endpoints. Each segment supplies normalized start/end times or a speed, an optional exit-time cue and an optional immutable `FloatCurve`.

- `Animation` clock preserves normalized intervals, including values above one.
- `DistanceSpeed` derives segment intervals from distance/speed and supplies a total duration to the native evaluator.
- Endpoints are snapshots. Resolve target offsets, radii and return anchors before preparation. Battle target expressions and formation updates are not evaluated.
- The state key is `Animator.StringToHash(stateName)`, not an execution serial.

Resolve bindings with `ResolveAnimationMoveData460`, then call `Prepare`. Each preparation allocates its own graph, segment list and segment objects. It retains every object and optional curve until `Close`. The SDK assembles these literal values directly; it does not invoke `SetupEntityMovement` or execute an official task graph.

Preparation does not move a character. It requires an engine-attached thread and a live metadata provider. Invalid intervals or speed-derived zero-length intervals fail before native allocation. Constructors and list operations can also report failure through `Error`.

## Component access

Construct `AnimationMovementAccess` with resolved bindings and the owned entity identity. Retain its movement component and supply a non-mutating predicate that checks the captured entity generation, world and model before each operation.

`Submit` requires an initialized, empty animation-movement channel with no other movement request or effect-attached motion. It calls the official submission entry once per prepared graph and reports the observed adoption. The game evaluates movement through its own animation clock and updates; the SDK does not run a second tick loop.

`Interrupt` compares the exact graph identity before clearing it. It leaves the current position and unrelated motion intact. It does not finish at the endpoint or restore formation-derived facing. A replacement graph, including one with the same animation key, is not cleared.

`Clock` reads the graph's animation and movement progress. `Detached` reports that the component no longer holds the graph; it does not establish natural completion. Failed native writes are not automatically retried.

Keep data and component owners alive through submission and cleanup. Call `Close` only after the component no longer consumes the graph, or after its owner has ended. Destructors do not call the engine.

## Completion observation

`AnimationMovementUpdates` observes the official evaluator for a registered component and exact graph. A normal evaluation that returns true and clears that graph reports completion. Animation mismatch returns false and reports interruption. Detachment outside this boundary does not imply completion.

The observer preserves all original arguments, output, return values and exceptions. Unregistered components and other threads are forwarded without reading their objects. Closing a watch disconnects it; the process-wide trampoline remains installed, so the SDK must not be unloaded while the game can call it.
