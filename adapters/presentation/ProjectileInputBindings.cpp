#include "ProjectileInputBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <optional>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
DeclaredField Field(ObjectMetadata& metadata, void* p, const std::string& name, const char* type) {
    std::optional<DeclaredField> selected;
    for (const auto& f : metadata.ReadObjectFields(p))
        if (f.name == name) {
            if (selected || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error("projectile input field differs: " + name);
            selected = f;
        }
    if (!selected)
        throw std::runtime_error("projectile input field absent: " + name);
    return *selected;
}
}
ProjectileInputSchema ResolveProjectileInputSchema(ObjectMetadata& metadata, void* source) {
    ProjectileInputSchema out;
    const auto type = metadata.ReadObjectTypeName(source);
    if (type.name_space != "RPG.GameCore")
        throw std::invalid_argument("projectile configuration namespace differs");
    out.legacy = type.name == "ProjectileData";
    if (out.legacy) {
        const auto behavior = Field(metadata, source, "Behavior", "RPG.GameCore.ProjectileMoveBehavior");
        int value = 0;
        if (!native::CopyMemoryChecked(&value, static_cast<char*>(source) + behavior.offset, sizeof(value)))
            throw std::runtime_error("projectile behavior unreadable");
        if (value != 1 && value != 2 && value != 4 && value != 7)
            throw std::invalid_argument("projectile behavior is not qualified");
        out.family = static_cast<ProjectileFamily>(value);
    } else if (type.name == "LinearProjectileData")
        out.family = ProjectileFamily::Linear;
    else if (type.name == "RandomTraceProjectileData")
        out.family = ProjectileFamily::RandomTrace;
    else
        throw std::invalid_argument("projectile configuration type is not qualified");

    for (const auto& f : metadata.ReadObjectFields(source))
        if (f.type == "RPG.GameCore.DynamicFloat" && f.flags_observed && !(f.flags & 0x10))
            out.numeric.push_back(f);
    out.hit_effect = Field(metadata, source, "HitEffect", "System.String");
    out.fly_effect = Field(metadata, source, "FlyEffect", "System.String");
    out.fly_unique_name = Field(metadata, source, "FlyEffectUniqueName", "System.String");
    out.align_hit = Field(metadata, source, "AlignHitEffect", "System.Boolean");
    out.attach_hit = Field(metadata, source, "AttachHitEffectToTargetEntity", "System.Boolean");
    out.start_from_last = Field(metadata, source, "StartFromLastHitPos", "System.Boolean");
    out.use_last_velocity = Field(metadata, source, "UseLastHitVelocity", "System.Boolean");
    out.trigger_hit_callback = Field(metadata, source, "TriggerHitCallback", "System.Boolean");
    out.enable_raycast = Field(metadata, source, "EnableRayCast", "System.Boolean");
    out.correct_formation = Field(metadata, source, "CorrectedWhenTargetFormationChange", "System.Boolean");
    return out;
}
std::set<std::string> ProjectileRequiredNumericInputs(ProjectileFamily family) {
    std::set<std::string> required{"FlySpeed", "FlyTime"};
    if (family == ProjectileFamily::Linear)
        required.insert("LinearPitchAngle");
    if (family == ProjectileFamily::HalfBoomerang) {
        required.insert("BoomerangEccentricity");
        required.insert("BoomerangAngleRoll");
    }
    if (family == ProjectileFamily::PrefabCurve)
        required.insert("SubCurveIndex");
    return required;
}
DeclaredField ResolveProjectileExpressionField(ObjectMetadata& metadata, void* dynamic) {
    return Field(metadata, dynamic, "postfixExpr", "JJAABNBJMEH");
}
Method ResolveProjectileInputClone(MethodResolver& m) {
    return m.Resolve("System", "Object", "MemberwiseClone", "System.Object", {});
}
Method ResolveProjectileDynamicFloat(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "DynamicFloat", "BIIMDAFMJNF", "RPG.GameCore.DynamicFloat", {"System.Single"},
                     MethodKind::Static);
}
Method ResolveProjectileEffectNameLength(MethodResolver& m) {
    return m.Resolve("System", "String", "get_Length", "System.Int32", {});
}
ComponentConfigMethods ResolveProjectileResourceText(MethodResolver& m) {
    ComponentConfigMethods out;
    out.string_length = ResolveProjectileEffectNameLength(m);
    out.string_character = m.Resolve("System", "String", "get_Chars", "System.Char", {"System.Int32"});
    return out;
}
}
