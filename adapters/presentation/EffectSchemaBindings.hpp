#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <functional>
namespace hksr::api::detail {
using EffectSchemaFields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
inline std::vector<DeclaredField> ReadEffectRelationsFields(const EffectSchemaFields& fields) {
    return fields("RPG.Client", "TriggerEffectParams");
}
inline std::vector<DeclaredField> ReadEffectRelationsFields(RuntimeMetadata& m) {
    return m.ReadFields("RPG.Client", "TriggerEffectParams");
}
std::size_t ResolveEffectInstanceRequestOffset(const std::vector<DeclaredField>&);
ObjectResult ResolveEffectControlType(ManagedObjects&, bool progress = false);
struct EffectRelationsLayout {
    std::size_t caster, target, param_entity, instance, id, released;
};
EffectRelationsLayout ResolveEffectRelationsLayout(const std::vector<DeclaredField>&);
std::size_t ResolveEffectTriggerPath(const std::vector<DeclaredField>&);
std::size_t ResolveEffectViewUiOffset(const EffectSchemaFields&);
std::pair<std::size_t, std::size_t> ResolveEffectViewPluginOffsets(const EffectSchemaFields&);
bool IsEffectFollowPlugin(TypeRelationships&, const RuntimeTypeName&);
inline bool IsEffectCameraSpace(bool ui, int position) {
    return ui || position == 2 || position == 4 || position == 6;
}
struct EffectQueryLayout {
    std::vector<DeclaredField> relations;
    std::size_t list, request, id;
};
EffectQueryLayout ResolveEffectQueryLayout(const EffectSchemaFields&);
ReferenceListMethods ResolveEffectQueryList(const DeclaredType&);
Method ResolveEffectQueryRange(MethodResolver&, const DeclaredType&);
Method ResolveEffectUniqueQuery(MethodResolver&);
ObjectResult ResolveEffectManagerType(ManagedObjects&);
ObjectResult NewEffectManagerListFieldName(ManagedObjects&);
}
