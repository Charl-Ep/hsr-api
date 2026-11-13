# Model rendering

`PresentationContext::CreateActorRendering` binds a current owned actor, world,
model and Art. Prepare the presentation context once; use the client on its
engine thread and call `Close` before releasing it.

| Operation | Contract |
|---|---|
| `Renderers(node)` | Current descendant renderer masks. An empty node selects the model root; a named node uses `AttachPointMapping`. |
| `Mask(snapshot, value)` | Writes the exact observed renderer in the same model epoch. |
| `Outline()` | Captures the current `CharacterEffect` extra add/remove lists. |
| `NamedRenderer(name)` | Resolves the official model renderer cache. |
| `SetOutline(snapshot, add, remove)` | Replaces both extra-target lists through the official setter. |
| `Visible(key, false)` | Adds a private rendering-only hide request. |
| `Visible(key, true)` | Removes that key only. |

Missing components return an empty observation; native observation failures throw.
Writes distinguish `Applied`, `Retired` and `Failed`. A failed write may be partial.

Mask and outline setters write shared model state. Consumers coordinate
overlapping contributions, retain their original values and withdraw them on
the same model before replacement.

Visibility keys affect rendering only. `Close` withdraws this client's remaining
keys and releases managed references. Callers restore raw mask and outline writes.
