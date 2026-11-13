#include "ModelScaleBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(RuntimeMetadata& m, const char* space, const char* type, const char* name, const char* value) {
    std::size_t offset = 0;
    unsigned count = 0;
    for (const auto& f : m.ReadFields(space, type, false))
        if (f.name == name && f.type == value && f.flags_observed && !(f.flags & 0x10)) {
            offset = f.offset;
            ++count;
        }
    if (count != 1)
        throw std::runtime_error(std::string("character scale field differs: ") + name);
    return offset;
}
RuntimeType FieldType(RuntimeMetadata& m, const char* space, const char* type, const char* name) {
    const auto owner = m.FindType(space, type);
    RuntimeField field;
    for (const auto& row : m.ObserveFields(owner).fields)
        if (row.error.empty() && row.declaration.name == name) {
            if (field)
                throw std::runtime_error("character scale field identity ambiguous");
            field = row.field;
        }
    if (!field)
        throw std::runtime_error("character scale field identity absent");
    return m.FieldType(field);
}
}
ModelScaleBindings ResolveModelScaleBindings(RuntimeMetadata& metadata, const ModelScaleRoot& keep) {
    ModelScaleBindings s;
    s.config_type = metadata.FindType("RPG.GameCore", "ScaleCharacterModel");
    s.helper_type = metadata.FindType("", "KOLOAICIFDM");
    s.helper_ctor = metadata.Resolve("", "KOLOAICIFDM", ".ctor", "System.Void",
                                     {"RPG.GameCore.TaskContext", "RPG.GameCore.ScaleCharacterModel"});
    s.apply = metadata.Resolve("", "KOLOAICIFDM", "PPFICACCHKB", "System.Void",
                               {"RPG.GameCore.GameEntity", "System.Single", "System.Single"});
    s.multiplier = metadata.Resolve("", "KOLOAICIFDM", "BPDGMAOEAFD", "System.Single", {"RPG.GameCore.GameEntity"});
    s.factors = Field(metadata, "RPG.GameCore", "CharacterModelComponent", "_RootScaleFactors",
                      "System.Collections.Generic.Dictionary<System.String,System.Single>");
    s.outline = Field(metadata, "RPG.Client", "CharacterShaderPropertyTransition", "LHGGJFKEGDA", "System.Single");
    s.config_outline = Field(metadata, "RPG.GameCore", "ScaleCharacterModel", "OutlineWidthScale", "System.Single");
    s.config_equal = Field(metadata, "RPG.GameCore", "ScaleCharacterModel", "EqualRatioByMin", "System.Boolean");
    const auto factors = FieldType(metadata, "RPG.GameCore", "CharacterModelComponent", "_RootScaleFactors");
    s.contains = metadata.Resolve(factors, "ContainsKey", "System.Boolean", {"System.String"});
    s.item = metadata.Resolve(factors, "get_Item", "System.Single", {"System.String"});
    s.multiplier_type = FieldType(metadata, "RPG.GameCore", "ScaleCharacterModel", "Multiplier");
    s.dict_ctor = metadata.Resolve(s.multiplier_type, ".ctor", "System.Void", {});
    s.dict_add = metadata.Resolve(s.multiplier_type, "Add", "System.Void", {"System.Single", "System.Single"});
    s.shader_get = metadata.Resolve("", "NPCEHHGJBNH", "NMGGBILIIMP", "RPG.Client.CharacterShaderPropertyTransition",
                                    {"RPG.GameCore.GameEntity"}, MethodKind::Static);
    s.alive = metadata.Resolve("UnityEngine", "Object", "op_Implicit", "System.Boolean", {"UnityEngine.Object"},
                               MethodKind::Static);
    s.factor_key =
        keep(metadata.NewStringUtf8("ScaleCharacterModel")); // exact gen73 literal, not inferred from task name
    auto* type = keep(metadata.ManagedType("RPG.GameCore", "ScaleCharacterModel"));
    auto* name = keep(metadata.NewStringUtf8("Multiplier"));
    const auto get = metadata.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                      {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult field;
    field.call = native::Call(get, &field.object, type, name, 54);
    s.multiplier_field = keep(field);
    const auto actual = metadata.ReadObjectType(field.object);
    MetadataResolver resolver({actual});
    s.multiplier_set = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                        {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                         "System.Reflection.Binder", "System.Globalization.CultureInfo"});
    return s;
}
}
