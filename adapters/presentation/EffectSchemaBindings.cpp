#include "EffectSchemaBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* ns, const char* owner, const char* name,
                  const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& field : fields)
        if (field.owner_namespace == ns && field.owner_name == owner && field.name == name) {
            if (found || field.type != type || !field.flags_observed || (field.flags & 16))
                throw std::invalid_argument(std::string("effect declaration differs: ") + owner + "." + name);
            found = &field;
        }
    if (!found)
        throw std::invalid_argument(std::string("effect declaration absent: ") + owner + "." + name);
    return found->offset;
}
}
std::size_t ResolveEffectInstanceRequestOffset(const std::vector<DeclaredField>& fields) {
    return Field(fields, "RPG.Client", "MonoEffect", "TriggerParams", "RPG.Client.TriggerEffectParams");
}
ObjectResult ResolveEffectControlType(ManagedObjects& objects, bool progress) {
    return objects.ManagedType(progress ? "RPG.Client" : "UnityEngine",
                               progress ? "MonoEffectPluginProgress" : "Animator");
}
EffectRelationsLayout ResolveEffectRelationsLayout(const std::vector<DeclaredField>& fields) {
    const auto get = [&](const char* name, const char* type) {
        return Field(fields, "RPG.Client", "TriggerEffectParams", name, type);
    };
    return {get("CasterEntity", "RPG.GameCore.GameEntity"),
            get("TargetEntity", "RPG.GameCore.GameEntity"),
            get("ParamEntity", "RPG.GameCore.GameEntity"),
            get("<EffectInstance>k__BackingField", "RPG.Client.MonoEffect"),
            get("<UniqueEffectID>k__BackingField", "System.UInt32"),
            get("<IsReleased>k__BackingField", "System.Boolean")};
}
std::size_t ResolveEffectTriggerPath(const std::vector<DeclaredField>& fields) {
    return Field(fields, "RPG.GameCore", "TriggerEffect", "EffectPath", "System.String");
}
std::size_t ResolveEffectViewUiOffset(const EffectSchemaFields& fields) {
    return Field(fields("RPG.Client", "MonoEffect"), "RPG.Client", "MonoEffect", "<IsUIEffect>k__BackingField",
                 "System.Boolean");
}
std::pair<std::size_t, std::size_t> ResolveEffectViewPluginOffsets(const EffectSchemaFields& fields) {
    return {Field(fields("RPG.Client", "MonoEffectBehavior"), "RPG.Client", "MonoEffectBehavior", "_EffectPluginList",
                  "RPG.PoolList<RPG.Client.MonoEffectPluginBase>"),
            Field(fields("RPG.Client", "MonoEffectPluginFollow"), "RPG.Client", "MonoEffectPluginFollow",
                  "PositionOption", "RPG.Client.PositionFollowOption")};
}
bool IsEffectFollowPlugin(TypeRelationships& relationships, const RuntimeTypeName& type) {
    return relationships.TypeDerivesFrom(type.name_space.c_str(), type.name.c_str(), "RPG.Client",
                                         "MonoEffectPluginFollow");
}
EffectQueryLayout ResolveEffectQueryLayout(const EffectSchemaFields& fields) {
    auto relations = fields("RPG.Client", "TriggerEffectParams");
    return {relations,
            Field(fields("RPG.Client", "MonoEffectManager"), "RPG.Client", "MonoEffectManager", "_EffectList",
                  "System.Collections.Generic.List<RPG.Client.MonoEffect>"),
            ResolveEffectInstanceRequestOffset(fields("RPG.Client", "MonoEffect")),
            Field(relations, "RPG.Client", "TriggerEffectParams", "<UniqueEffectID>k__BackingField", "System.UInt32")};
}
ReferenceListMethods ResolveEffectQueryList(const DeclaredType& type) {
    return ResolveReferenceListMethods(type, "RPG.Client.MonoEffect");
}
Method ResolveEffectQueryRange(MethodResolver& resolver, const DeclaredType& type) {
    return resolver.Resolve(type.name_space.c_str(), type.name.c_str(), "GetRange",
                            "System.Collections.Generic.List<RPG.Client.MonoEffect>", {"System.Int32", "System.Int32"});
}
Method ResolveEffectUniqueQuery(MethodResolver& m) {
    return m.Resolve("RPG.Client", "MonoEffectManager", "FindEffectByUniqueName", "RPG.Client.MonoEffect",
                     {"System.String", "RPG.GameCore.GameEntity"});
}
ObjectResult ResolveEffectManagerType(ManagedObjects& objects) {
    return objects.ManagedType("RPG.Client", "MonoEffectManager");
}
ObjectResult NewEffectManagerListFieldName(ManagedObjects& objects) {
    return objects.NewStringUtf8("_EffectList");
}
}
