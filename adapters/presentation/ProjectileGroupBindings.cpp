#include "ProjectileGroupBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <optional>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const ProjectileGroupServices& s, const char* ns, const char* type, const char* name,
                  const char* expected) {
    std::optional<std::size_t> found;
    for (const auto& f : s.fields(ns, type))
        if (f.name == name) {
            if (found || !f.flags_observed || (f.flags & 0x10) || f.type != expected)
                throw std::runtime_error(std::string("projectile field differs: ") + name);
            found = f.offset;
        }
    if (!found)
        throw std::runtime_error(std::string("projectile field absent: ") + name);
    return *found;
}
}
ProjectileGroupBindings ResolveProjectileGroupBindings(const ProjectileGroupServices& services) {
    ProjectileGroupBindings out;
    auto& m = *services.methods;
    out.component_ctor = m.Resolve("RPG.GameCore", "AbilityComponent", ".ctor", "System.Void", {});
    out.owner_ref =
        m.Resolve("RPG.GameCore", "GameComponentBase", "InitOwnerRef", "System.Void", {"RPG.GameCore.GameEntity"});
    out.config_ctor = m.Resolve("RPG.GameCore", "AdventureAbilityConfig", ".ctor", "System.Void", {});
    out.ability_ctor = m.Resolve("", "AEEKMHCMBPL", ".ctor", "System.Void",
                                 {"RPG.GameCore.AbilityComponent", "RPG.GameCore.AdventureAbilityConfig"});
    out.ability_dispose = m.Resolve("", "AEEKMHCMBPL", "Dispose", "System.Void", {});
    out.context_get = m.Resolve("", "HPICNGFLLCA", "NNEMNIPFKGA", "RPG.GameCore.TaskContext", {});
    out.skill_ctor = m.Resolve("", "OHHPOGMFPBF", ".ctor", "System.Void", {});
    out.list_get = m.Resolve("", "HPICNGFLLCA", "IJJNBFPJDDP", "System.Collections.Generic.List<NAOKFLDEDBJ>", {});
    out.tick = m.Resolve("", "HPICNGFLLCA", "DEGNGOFGEJB", "System.Void", {"System.Single"});
    out.carrier_ctor =
        m.Resolve("", "FBCJBMKPDEH", ".ctor", "System.Void",
                  {"RPG.GameCore.ProjectileData", "HPICNGFLLCA", "RPG.GameCore.GameEntity", "RPG.GameCore.GameEntity",
                   "NBHAKGBHPOE", "OHHPOGMFPBF", "LACOFPCJAFI", "System.Boolean", "RPG.GameCore.NewProjectileConfig"});
    out.carrier_dispose = m.Resolve("", "NAOKFLDEDBJ", "Dispose", "System.Void", {});
    out.partial_cleanup = m.Resolve("", "FBCJBMKPDEH", "KDONLCELGKG", "System.Void", {});
    out.previous_set = m.Resolve("", "NAOKFLDEDBJ", "set_PreviousProjectile", "System.Void", {"NAOKFLDEDBJ"});
    out.wrapper = Field(services, "", "NAOKFLDEDBJ", "FCKCHPNEEKB", "UnityEngine.GameObject");
    out.position = Field(services, "", "NAOKFLDEDBJ", "AKOIKFDLNID", "UnityEngine.Vector3");
    out.velocity = Field(services, "", "NAOKFLDEDBJ", "CNCEDLKOHFL", "UnityEngine.Vector3");
    out.hit_position = Field(services, "", "NAOKFLDEDBJ", "HFCLFJMMFJK", "UnityEngine.Vector3");
    out.direction = Field(services, "", "NAOKFLDEDBJ", "HFOMPEEAPCG", "UnityEngine.Vector3");
    out.phase = Field(services, "", "NAOKFLDEDBJ", "<State>k__BackingField", "RPG.GameCore.ProjectileState");
    out.ordered = Field(services, "", "NAOKFLDEDBJ", "<EnsureCallbackOrder>k__BackingField", "System.Boolean");
    out.last_position =
        Field(services, "RPG.GameCore", "AbilityComponent", "_LastFinishProjectilePos", "UnityEngine.Vector3");
    out.last_velocity =
        Field(services, "RPG.GameCore", "AbilityComponent", "_LastFinishProjectileVelocity", "UnityEngine.Vector3");
    out.owner_kind = Field(services, "RPG.GameCore", "GameEntity", "_EntityType", "RPG.GameCore.EntityType");
    const char* tasks[] = {"OnStart", "OnAdd", "OnRemove", "OnAbort"};
    for (std::size_t i = 0; i < out.ability_tasks.size(); ++i)
        out.ability_tasks[i] =
            Field(services, "RPG.GameCore", "AdventureAbilityConfig", tasks[i], "RPG.GameCore.TaskConfig[]");
    return out;
}
ObjectResult AllocateProjectileObject(ManagedObjects& objects, ProjectileObject role) {
    switch (role) {
    case ProjectileObject::Component:
        return objects.AllocateObject("RPG.GameCore", "AbilityComponent");
    case ProjectileObject::Configuration:
        return objects.AllocateObject("RPG.GameCore", "AdventureAbilityConfig");
    case ProjectileObject::Ability:
        return objects.AllocateObject("", "AEEKMHCMBPL");
    case ProjectileObject::SkillContext:
        return objects.AllocateObject("", "OHHPOGMFPBF");
    case ProjectileObject::Carrier:
        return objects.AllocateObject("", "FBCJBMKPDEH");
    }
    throw std::invalid_argument("unknown projectile allocation role");
}
void SetProjectileSkillContext(const ProjectileGroupServices& services, void* receiver, void* context,
                               const std::function<void*(ObjectResult)>& retain) {
    Field(services, "", "OHHPOGMFPBF", "CGDCONMACEN", "RPG.GameCore.TaskContext");
    const char* ns = "";
    const char* type = "OHHPOGMFPBF";
    const char* name = "CGDCONMACEN";
    const auto t = services.objects->ManagedType(ns, type), n = services.objects->NewStringUtf8(name);
    retain(t);
    retain(n);
    const auto find = services.methods->Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                                {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult field;
    field.call = native::Call(find, &field.object, t.object, n.object, 54);
    auto* f = retain(field);
    const auto actual = services.metadata->ReadObjectType(f);
    MetadataResolver methods({actual});
    const auto set = methods.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                     {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                      "System.Reflection.Binder", "System.Globalization.CultureInfo"});
    if (!native::CallVoid(set, f, receiver, context, 0, static_cast<void*>(nullptr), static_cast<void*>(nullptr))
             .returned)
        throw std::runtime_error("private projectile context write failed");
}
std::pair<Method, Method> ResolveProjectileListMethods(ObjectMetadata& metadata, void* list) {
    const auto type = metadata.ReadObjectType(list);
    MetadataResolver methods({type});
    return {methods.Resolve(type.name_space.c_str(), type.name.c_str(), "Add", "System.Void", {"NAOKFLDEDBJ"}),
            methods.Resolve(type.name_space.c_str(), type.name.c_str(), "Contains", "System.Boolean", {"NAOKFLDEDBJ"})};
}
bool IsProjectileOwnerKind(int kind) noexcept {
    return kind == 6;
}
bool ProjectileHasArrived(int state) noexcept {
    return state >= 2;
}
}
