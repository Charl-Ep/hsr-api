# Projectiles

`PresentationContext::CreateProjectiles` creates a private visual emission group for an owned Story actor. Supported parameter families are Linear, RandomTrace, HalfBoomerang and prefab curves.

Read a parameter object with `OfficialCatalog::ReadProjectileTemplate`. Its `DefinitionSource` selects an ability, global modifier or global task template and a declared reference path. The returned `ProjectileTemplate` retains that object until `Close`. Each `Fire` creates a private parameter copy; required dynamic expressions need explicit evaluated numeric values.

Call `Prepare`, then `Fire` with parameters and a target captured by `CaptureProjectileTarget`. Service the group once per official frame. `Finish` stops further emissions and drains natural tails. `Cancel` stops the group's wrappers and drains its owned resources. Keep the group alive until `Status().closed`, including after partial construction failure.

The group uses the official flight evaluator and projectile-list update, with collision disabled on its wrapper. Hit effects use separately owned requests. Arrival receipts report carrier position/velocity and visual hit position/direction separately. Gameplay callbacks remain outside this visual service.

Cancel and drain before replacing either participant's model. Each request captures model identity, world identity, entity generation and its own effect receipts.

The service supports ordinary effect-prefab hits. Unsupported paths are the alternate hit-resource entity factory, arbitrary target selectors and nested `OnProjectileHit` tasks. Validate flight and impact rendering for the selected game resources.
