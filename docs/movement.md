# Animation movement

`PresentationContext::StartMovement` submits one animation-driven movement on an owned entity. It captures the current position, model and controller, validates the named base-layer state and returns an `OwnedMovementRequest`. It does not play the animation.

The entity must already have an initialized native animation-movement component. The factory does not construct that component. Story-created actors currently lack it; switching their model to a battle controller does not add it.

Service the request on the engine thread. Cancel and drain it before a model handoff. The request also cancels at entity pre-disposal and detects unexpected model changes. Completion, interruption, replacement, cancellation and released references have separate status fields.

`AnimationMovementData` and `AnimationMovementAccess` are the lower-level data and component adapters.

## Data

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
