# SDK modules

Public headers are under `include/hksr/<module>`. Implementations and tests use the same capability groups.

| Module | Responsibility |
|---|---|
| `math` | Position and quaternion values |
| `metadata`, `managed` | IL2CPP declarations, managed inputs and reference ownership |
| `runtime`, `world` | Runtime clients, official frames, map identity and transforms |
| `entity` | Map-NPC, story and partner creation, entity generations and retirement |
| `model` | Model observations, Art admission, baseline capture and model transitions |
| `animation` | Animator and FreeStyle access, controller resources and source catalogs |
| `presentation/events` | Event registrations, baseline supplements and private configuration projection |
| `presentation/timeline` | Timeline graphs, registries and playback ownership |
| `presentation/effects` | Effect requests, completion and cleanup |
| `presentation/audio` | AudioManager requests, callback receipts and cancellation |
| `control` | Placement, grounding, gaze, partner relations and mouse mode |
| `unity` | Components, meshes, materials, renderers and resource lifetimes |
| `platform` | Native hook infrastructure |
| `serialization` | Unity component-document encoding and reference remapping |

## Composition

`PresentationContext::Create` resolves bindings and exposes capability factories. Sessions retain their own resources; `ModelServices` and `PresentationResources` provide shared call interfaces.

Private `PresentationCallScope` carries actor provenance across nested effects,
tasks and Timeline callbacks; the Timeline registry owns playback records.

`ActorAudioHost` owns actor sound mappings, emitter identity, request services and audio retirement. `ActorSkinBindings` owns prepared skins and their exact entity-generation bindings. `PresentationContext` composes and closes these owners; `BindingErrors()` retains binding and preparation failures independently of tracing.

`animation/ActorEmotion.hpp` provides local emotion setup and its scoped initialization
boundary through `Hksr::Api`. Actor input substitution is a private model service.

Visual configuration has four layers: the public request validates typed input;
`adapters/model/VisualConfigurationBindings` resolves the game schema;
`ActorVisualConfigurationNative` owns reflection inputs and native writes;
`VisualConfigurationHost` captures owned component identities and type roots.
`PresentationContext` delegates creation and shutdown to the host.

`adapters/animation/catalog` decodes official tables and configuration schemas. `src/animation/catalog` owns managed references, traversal queues, caching, cancellation and progressive reads. `OfficialCatalog` is the public facade.

Adapters hold current declaration mappings, native layouts and call adaptation. Shared registries and lifecycle logic remain in `src`. Each capability binds its own contract through one current adapter; see [version adaptation](version-adaptation.md).

## Boundaries

Public headers define typed inputs, results and ownership. Private composition, hooks and codecs remain in `src`. [NativeSupport](native-support.md) is a separate static package.

Spatial values are defined in `math/SpatialTypes.hpp`; model observations in `model/ModelAccess.hpp`.

JSON is confined to `src/serialization/ComponentReferences.cpp`. It preserves Unity's component document while remapping local references. Official `JsonConfig` classes are accessed through managed metadata.

`sdk.module-boundaries` checks these source dependencies. Consumers provide UI, saved intent and playback policy.
