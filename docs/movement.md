# Animation movement

## Facing

`PresentationContext::FaceEntity` turns an owned actor toward a world point using the shortest horizontal angle, a clamped ratio and an angle offset. Positive duration uses official Transform smoothing; nonpositive duration applies immediately. The return value confirms submission. A later turn replaces the target. Position and navigation policy remain unchanged.

## Animation-driven movement

`PresentationContext::StartMovement` submits one animation-driven movement on an owned entity. It captures the current position, model and controller, validates the named base-layer state and returns an `OwnedMovementRequest`. Start the body animation separately.

For Story actors, the context adds the official animation-movement component on demand using a cached closed generic factory. Component registration, model callbacks and updates use the game's lifecycle.

`AcquireMovementHost` returns an exclusive lease shared by the animation-movement and effect-follow services. The lease pauses ordinary Adventure root translation and the movement component's raw root callback. It leaves animation and IK callbacks running. `Ready` waits for previously submitted Adventure movement to drain; `waiting_for_host` reports this state on an animation request. The request samples its origin after that wait.

Close the lease after detaching its motion channels and before changing the model. Closing restores the original motion policy and removes a component created by that lease; existing components are retained. Entity retirement ends native access. Cleanup requires explicit servicing.

Service the request on the engine thread. Cancel and drain it before a model handoff. The request also cancels at entity pre-disposal and detects unexpected model changes. Completion, interruption, replacement, cancellation and released references have separate status fields.

`AnimationMovementData` and `AnimationMovementAccess` are the lower-level data and component adapters.

## Data

`PresentationContext::StartPosition` applies a resolved world point through the official movement component and TransformComponent. It returns the same serviced request type and uses the exclusive motion lease. Pending Adventure movement drains before submission. A successful call reports `completed`; cancellation before submission leaves position unchanged. This operation accepts a position independently of an animation state.

`ReadMovementGeometry` reads radius and local target offset from an existing `CharacterDataComponent`, or returns unavailable when absent. `OfficialCatalog::ReadCharacterVisualRadius` reads the selected configuration's base radius. Movement geometry and collision bounds are separate inputs.

An `AnimationMovePlan` contains an origin, an Animator state key and ordered segments with resolved world-space endpoints. Each segment supplies normalized start/end times or a speed, an optional exit-time cue and an optional immutable `FloatCurve`.

- `Animation` clock preserves normalized intervals, including values above one.
- `DistanceSpeed` derives segment intervals from distance/speed and supplies a total duration to the native evaluator.
- Endpoints are snapshots. Resolve target expressions, offsets, radii and return anchors before preparation.
- The state key is `Animator.StringToHash(stateName)`.

Resolve bindings with `ResolveAnimationMoveData`, then call `Prepare`. Each preparation builds a private graph, segment list and segment objects from the supplied values, retaining them and any curves until `Close`.

Preparation requires an engine-attached thread and a live metadata provider. Submission is a separate step. Invalid intervals or speed-derived zero-length intervals fail before native allocation. Constructors and list operations can also report failure through `Error`.

## Component access

Construct `AnimationMovementAccess` with resolved bindings and the owned entity identity. Retain its movement component and supply a non-mutating predicate that checks the captured entity generation, world and model before each operation.

`Submit` requires an initialized, empty animation-movement channel with no competing movement request or effect-attached motion. It calls the official submission entry once per prepared graph and reports adoption. The game evaluates movement through its animation clock and updates.

`Interrupt` clears the captured graph when its identity still matches, preserving current position and unrelated or replacement motion. To return to an endpoint or facing, submit that position or turn explicitly.

`Clock` reads animation and movement progress. `Detached` reports that the component released the graph; use completion observation to distinguish completion from interruption. Failed native writes are not automatically retried.

Keep data and component owners alive through submission and cleanup. Call `Close` only after the component no longer consumes the graph, or after its owner has ended. Destructors do not call the engine.

## Completion observation

`AnimationMovementUpdates` observes the official evaluator for a registered component and exact graph. A normal evaluation returning true and clearing that graph reports completion. Animation mismatch returns false and reports interruption. Other detachment is reported separately.

The observer preserves original arguments, output, return values and exceptions. It inspects only registered components on the owning thread. Closing a watch disconnects it; the process-wide trampoline remains installed for the SDK's process lifetime.
