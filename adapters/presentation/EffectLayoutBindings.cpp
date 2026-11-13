#include "EffectLayoutBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error("layout field declaration differs");
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("layout field absent: ") + name);
    return found->offset;
}
}
EffectLayoutNativeBindings ResolveEffectLayoutBindings(RuntimeMetadata& m,
                                                       const std::function<void*(ObjectResult)>& retain) {
    EffectLayoutNativeBindings out;
    auto* s = &out;
    s->plugin_type = retain(m.ManagedType("RPG.Client", "Effect_MahJongAutoLayout"));
    const auto plugin = m.ReadFields("RPG.Client", "Effect_MahJongAutoLayout"),
               driver = m.ReadFields("", "CIIFBLMIPGP");
    s->trigger = Field(m.ReadFields("RPG.Client", "MonoEffect"), "MonoEffect", "TriggerParams",
                       "RPG.Client.TriggerEffectParams");
    s->plugin_owner = Field(plugin, "MonoEffectPluginBase", "_OwnerEffect", "RPG.Client.MonoEffect");
    s->behavior = Field(plugin, "Effect_MahJongAutoLayout", "KADDKEGNCHE", "CIIFBLMIPGP");
    s->behavior_owner = Field(driver, "BehaviorBase", "_OwnerMonoBehavior", "UnityEngine.MonoBehaviour");
    s->enabled = Field(plugin, "Effect_MahJongAutoLayout", "IsLayoutEnable", "System.Boolean");
    s->hidden = Field(plugin, "Effect_MahJongAutoLayout", "IsHideLayout", "System.Boolean");
    s->fade = Field(plugin, "Effect_MahJongAutoLayout", "NeedFadein", "System.Boolean");
    Field(plugin, "Effect_MahJongAutoLayout", "InheriteLayout", "RPG.Client.Effect_MahJongAutoLayout");
    s->component = m.Resolve("UnityEngine", "Component", "GetComponent", "UnityEngine.Component", {"System.Type"});
    s->alive =
        m.Resolve("UnityEngine", "Object", "op_Implicit", "System.Boolean", {"UnityEngine.Object"}, MethodKind::Static);
    const char* perform[] = {"NFDPAOKJFMN", "LGEAAHBLPEG", "PEMICJFNMHP"};
    for (int i = 0; i < 3; ++i)
        s->perform[i] = m.Resolve("", "CIIFBLMIPGP", perform[i], "System.Void", {});
    s->perform[3] = m.Resolve("", "CIIFBLMIPGP", "LHMGAMGBHDF", "System.Void", {"System.Boolean"});
    s->copy = m.Resolve("", "CIIFBLMIPGP", "LFINAIMIDEM", "System.Void", {"RPG.Client.Effect_MahJongAutoLayout"});
    s->clear = m.Resolve("", "CIIFBLMIPGP", "DCDEEALNBKC", "System.Void", {});
    auto* name = retain(m.NewStringUtf8("InheriteLayout"));
    const auto find = m.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult field;
    field.call = native::Call(find, &field.object, s->plugin_type, name, 52);
    s->inherit_field = retain(field);
    const auto actual = m.ReadObjectType(field.object);
    MetadataResolver resolver({actual});
    s->inherit_set = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                      {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                       "System.Reflection.Binder", "System.Globalization.CultureInfo"});
    return out;
}
}
