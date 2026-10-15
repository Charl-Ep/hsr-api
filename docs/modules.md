# SDK modules

Public headers are under `include/hksr/<module>`. Implementations and tests use the same capability groups.

| Module | Responsibility |
|---|---|
| `metadata`, `managed` | IL2CPP declarations, managed inputs and reference ownership |
| `runtime`, `world` | Runtime clients, official frames, map identity and transforms |
| `entity` | ClientCreateMapNPC inputs, entity generations and retirement |
| `model` | CharacterModelComponent, Art admission, baseline capture and model transitions |
| `animation` | Animator and FreeStyle access, controller resources and source catalogs |
| `presentation/events` | Event registrations, baseline supplements and private configuration projection |
| `presentation/timeline` | Timeline graphs, registries, call scopes and ownership |
| `presentation/effects` | Effect requests, completion and cleanup |
| `control` | Placement, grounding, gaze and mouse mode |
| `unity` | Components, meshes, materials, renderers and resource lifetimes |
| `platform` | Native hook infrastructure |
| `serialization` | Unity component-document encoding and reference remapping |

## Composition

`PresentationContext::Create` resolves bindings and exposes capability factories. Sessions retain their own resources; `ModelServices` and `PresentationResources` provide shared call interfaces.

`src/animation/catalog` implements character tables, action/event configuration, ready defaults, ability graphs and NPC motion queries. `CatalogReader.hpp` is private collection and reference infrastructure.

Adapters hold declaration names, native layouts and runtime profiles. Shared registries and lifecycle logic remain in `src`. `RuntimeVersion` selects a profile; automatic startup recognizes game 4.6.

## Boundaries

Public headers define typed inputs, results and ownership. Private composition, hooks and codecs remain in `src`. [NativeSupport](native-support.md) is a separate static package.

JSON is confined to `src/serialization/ComponentReferences.cpp`. It preserves Unity's component document while remapping local references. Official `JsonConfig` classes are accessed through managed metadata.

`sdk.module-boundaries` checks these source dependencies. Consumers provide UI, saved intent and playback policy.
