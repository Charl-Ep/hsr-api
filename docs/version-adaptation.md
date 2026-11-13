# Game adaptation

`adapters/` contains game declaration mappings, configuration decoding, native
layouts, call bridges and code locators. One current adapter, organized by
capability, supplies the SDK's version-independent interfaces.

| Domain | Adaptation inputs |
|---|---|
| `runtime` | Loaded modules, metadata table, initialization and thread entry points |
| `animation`, `animation/catalog` | Animator layouts, component bindings and official data schemas |
| `entity`, `model`, `control` | Factories, component identities, model configuration and control calls |
| `presentation` | Events, effects, audio, movement, projectiles and Timeline contracts |
| `unity`, `world` | Engine declarations, native mesh writers and world frame access |

Locators derive addresses from loaded code and metadata. Other names and layouts
are explicit adapter inputs. Each capability is qualified by its callable,
schema and cleanup contracts; image identity is diagnostic metadata.

An unresolved binding affects its dependent operations. Independent capabilities
remain available. Resource creation requires its matching cleanup contract.
Method resolution requires a unique matching callable and reports unread
declarations as diagnostics.

## Updating

1. Check the affected adapter's declarations, code relations and value ABI.
2. Change its mapping or bridge while preserving the public semantic contract.
3. Run its fixtures, including missing unrelated bindings and partial cleanup.
4. Verify construction, playback, interruption and retirement in the game.

Managed ownership, request state and scheduling remain in `src`. Preserving the
public contract keeps consumer source compatible. Incompatible C++ contract
changes require a new interface generation and consumer rebuild. Authored
resources and task catalogs have separate content identities.
