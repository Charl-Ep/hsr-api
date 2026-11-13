# Runtime host

Register a `RuntimeClient` outside `DllMain` with `RegisterRuntimeClient(client)`. The SDK binds the loaded runtime and returns a move-only `RuntimeSubscription`.

Clients share one process-resident host and world hook.

## Callbacks

| Callback | Thread | Responsibility |
|---|---|---|
| `Prepare` | Engine-attached metadata worker | Resolve declarations and construct native services |
| `BeforeFrame` | Official world thread | Open an optional frame scope |
| `Frame` | Official world thread | Advance work or drain ownership when stopping |
| `AfterFrame` | Official world thread | Finish the frame scope |

Use `Prepare` for declarations and native services, then create scene objects in official-frame callbacks. Preparation is serialized against client frame delivery. Official updates skip SDK delivery while preparation holds the dispatcher; host status counts these skipped frames.

For each batch, the host calls all `BeforeFrame` callbacks, then all `Frame` callbacks in registration order, then `AfterFrame` in reverse order. Stop requests and new registrations take effect in a later batch. Callbacks run outside registration/status locks. Complete registration and stop waits outside callbacks to avoid blocking dispatch.

Exceptions are recorded per client. Other clients and final callbacks still run. Returning `true` from `Frame` declares ownership drained; the host releases the client after its `AfterFrame` also succeeds.

## Shutdown

Keep the subscription while the client is active. `RequestStop()` stops only that client. Destruction also requests stop without waiting.

After successful preparation, cleanup requires official frames. A stop racing with preparation still delivers a stop frame. Cancellation before preparation releases the pending client without a frame. Observe `Status()` until `Stopped` or preparation `Failed`.

The empty host remains available for later clients. Hooks and runtime infrastructure stay loaded. Keep client code and separately retained callback owners alive through completion; hot-unloading is unsupported.

## Scoped operation timing

`OperationTrace` enables synchronous, thread-local collection for one operation; an empty sink disables it. `OperationSpan` records nested stages. SDK invocation wrappers record native call targets, module names/RVAs, counts and unsuccessful call receipts. This tracing is provided by the DLL's invocation wrappers.

The sink receives aggregate rows after collection ends. Each row contains its parent, inclusive duration, self duration and maximum call duration. Self time subtracts instrumented children and includes uninstrumented work and collection overhead. Use inclusive time for a whole stage or self time for exclusive totals. These are synchronous wall-clock measurements; asynchronous loading and CPU/GPU profiling need separate instrumentation. Module description and sink delivery occur after the timed root ends. Native call failures include unavailable bindings and exceptions.

Reports retain no game objects. Collection and sink failures do not change operation results. Keep a trace and its spans on one thread with normal nested scope lifetimes.

## Status and metadata

`ReadRuntimeHostStatus()` reports shared startup, thread attachment and frame counters. Subscription status reports that client's preparation, frame errors and completion.

Metadata caches publish copied values under short locks; native reads occur outside those locks. Game-object access still requires the owning engine thread.

The preparation worker detaches before frame delivery and holds no engine attachment while idle. An uncertain detach disables further attachment attempts on that worker. Infrastructure startup failure is terminal; individual preparation failures affect only their registration.
