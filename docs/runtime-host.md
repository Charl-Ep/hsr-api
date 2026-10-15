# Runtime host

Register a `RuntimeClient` outside `DllMain` with `RegisterRuntimeClient(client)`. The SDK selects the loaded-module profile and returns a move-only `RuntimeSubscription`. The explicit `RuntimeVersion` overload rejects a profile that conflicts with an existing host.

Clients share one process-resident host and world hook.

## Callbacks

| Callback | Thread | Responsibility |
|---|---|---|
| `Prepare` | Engine-attached metadata worker | Resolve declarations and construct native services |
| `BeforeFrame` | Official world thread | Open an optional frame scope |
| `Frame` | Official world thread | Advance work or drain ownership when stopping |
| `AfterFrame` | Official world thread | Finish the frame scope |

`Prepare` must not create scene objects. Preparation is serialized against client frame delivery. Official updates skip SDK delivery while preparation holds the dispatcher; skipped frames are counted in host status.

For each batch, the host calls all `BeforeFrame` callbacks, then all `Frame` callbacks in registration order, then `AfterFrame` in reverse order. Stop requests and new registrations take effect in a later batch. Callbacks run without registration/status locks; do not wait inside them for another registration or stop to complete.

Exceptions are recorded per client. Other clients and final callbacks still run. Returning `true` from `Frame` declares ownership drained; the host releases the client after its `AfterFrame` also succeeds.

## Shutdown

Keep the subscription while the client is active. `RequestStop()` stops only that client. Destruction also requests stop without waiting.

After successful preparation, cleanup requires official frames. A stop racing with preparation still delivers a stop frame. Cancellation before preparation releases the pending client without a frame. Observe `Status()` until `Stopped` or preparation `Failed`.

The empty host remains available for later clients. Hooks and runtime infrastructure stay loaded. Keep client code and separately retained callback owners alive through completion; hot-unloading is unsupported.

## Status and metadata

`ReadRuntimeHostStatus()` reports shared startup, thread attachment and frame counters. Subscription status reports that client's preparation, frame errors and completion.

Metadata caches publish copied values under short locks; native reads occur outside those locks. Cache synchronization does not make arbitrary game-object access thread-safe.

The preparation worker detaches before frame delivery and holds no engine attachment while idle. An uncertain detach disables further attachment attempts on that worker. Infrastructure startup failure is terminal; individual preparation failures affect only their registration.
