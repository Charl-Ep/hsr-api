#include "EventValueBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <stdexcept>
namespace hksr::api::detail {
EventValueMethods ResolveEventValueMethods(MethodResolver& m) {
    return {m.Resolve("", "LGBHONLHNPN", "get_TaskContext", "RPG.GameCore.TaskContext", {}),
            m.Resolve("RPG.GameCore", "TaskContext", "GetOwnerCustomData", "JBFFCBPMKEF", {}),
            m.Resolve("", "FODPNNIJCJI", ".ctor", "System.Void", {}),
            m.Resolve("", "FODPNNIJCJI", "GDEJIHHKNBI", "System.Void", {"ENEIHJJPNPI"}),
            m.Resolve("", "FODPNNIJCJI", "CMEAAFAOMNJ", "System.Void",
                      {"System.Collections.Generic.Dictionary<System.String,System.String>"}),
            m.Resolve("RPG.GameCore", "GameCoreConfigLoader", "LoadCharacterConfig", "RPG.GameCore.CharacterConfig",
                      {"System.String"}, MethodKind::Static)};
}
EventValueFieldBinding ResolveEventValueField(MethodResolver& methods, ObjectMetadata& metadata,
                                              ManagedObjects& objects, EventValueField role,
                                              const std::function<void*(ObjectResult)>& retain) {
    const char* ns = nullptr;
    const char* type = nullptr;
    const char* name = nullptr;
    bool stat = false;
    switch (role) {
    case EventValueField::Numeric:
        ns = "";
        type = "JBFFCBPMKEF";
        name = "KFKGOMIMNLD";
        break;
    case EventValueField::Strings:
        ns = "";
        type = "JBFFCBPMKEF";
        name = "KINCCPGLBEC";
        break;
    case EventValueField::GlobalConfiguration:
        ns = "RPG.GameCore";
        type = "GameCoreConfigManager";
        name = "GameCoreConstValueConfig";
        stat = true;
        break;
    case EventValueField::GlobalNumericDefaults:
        ns = "RPG.GameCore";
        type = "GameCoreConstValue";
        name = "AllCharacterDynamicValues";
        break;
    case EventValueField::CharacterNumericDefaults:
        ns = "RPG.GameCore";
        type = "CharacterConfig";
        name = "DynamicValues";
        break;
    case EventValueField::CharacterStringDefaults:
        ns = "RPG.GameCore";
        type = "CharacterConfig";
        name = "DynamicStrings";
        break;
    }
    if (!name)
        throw std::invalid_argument("unknown event value field");
    auto* managed = retain(objects.ManagedType(ns, type));
    auto* text = retain(objects.NewStringUtf8(name));
    ObjectResult result;
    result.call = native::Call(methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                               {"System.String", "System.Reflection.BindingFlags"}),
                               &result.object, managed, text, stat ? 56 : 52);
    auto* field = retain(result);
    const auto info = metadata.ReadObjectType(field);
    MetadataResolver resolve({info});
    return {field,
            resolve.Resolve(info.name_space.c_str(), info.name.c_str(), "GetValue", "System.Object", {"System.Object"}),
            resolve.Resolve(info.name_space.c_str(), info.name.c_str(), "SetValue", "System.Void",
                            {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                             "System.Reflection.Binder", "System.Globalization.CultureInfo"})};
}
ObjectResult AllocateEventValueStage(ManagedObjects& objects) {
    return objects.AllocateObject("", "FODPNNIJCJI");
}
Method ResolveEventStringDictionaryCopy(ObjectMetadata& metadata, void* dictionary) {
    const auto actual = metadata.ReadObjectType(dictionary);
    MetadataResolver resolver({actual});
    return resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), ".ctor", "System.Void",
                            {"System.Collections.Generic.IDictionary<RPG.GameCore.StringHash,System.String>"});
}
}
