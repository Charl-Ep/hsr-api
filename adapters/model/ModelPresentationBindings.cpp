#include "ModelPresentationBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
bool PrepareModelFollowFactory(GenericFactory& factory, void* owner_type, void* follow_type) {
    return factory.Prepare(owner_type, "CreateComponent", follow_type);
}
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* space, const char* owner, const char* name,
                  const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == space && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("model presentation field differs: ") + name);
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("model presentation field missing: ") + name);
    return found->offset;
}
}
ModelPresentationBindings ResolveModelPresentationBindings(RuntimeMetadata& m, const ModelPresentationType& type,
                                                           const ModelPresentationFollowFactory& prepare_follow) {
    ModelPresentationBindings s;
    s.model_type = type("RPG.GameCore", "CharacterModelComponent");
    try {
        s.face_type = type("", "DEGDOEMPKCM");
        const auto fields = m.ReadFields("", "DEGDOEMPKCM", false);
        s.cheek = Field(fields, "", "DEGDOEMPKCM", "FIJCHDIEKGF", "System.Single");
        s.shy = Field(fields, "", "DEGDOEMPKCM", "IFJMLKGEBKD", "System.Single");
        s.shadow = Field(fields, "", "DEGDOEMPKCM", "FEKKGMEBEBG", "System.Single");
        s.face_cache = Field(fields, "", "DEGDOEMPKCM", "EAIBBECPIFJ", "RPG.Client.RendererMaterialCache");
        s.face_set = m.Resolve("", "DEGDOEMPKCM", "BPDFPHNAJOE", "System.Void",
                               {"System.Single", "System.Single", "System.Single", "System.Single"});
    } catch (const std::exception& error) {
        s.face_error = error.what();
    }
    try {
        s.scale_set = m.Resolve("RPG.GameCore", "CharacterModelComponent", "SetRootScaleFactor", "System.Void",
                                {"System.String", "System.Single", "System.Boolean"});
        s.scale_read = m.Resolve("RPG.GameCore", "CharacterModelComponent", "get_RootScale", "System.Single", {});
    } catch (const std::exception& error) {
        s.scale_error = error.what();
    }
    try {
        s.follow_type = type("", "KLKEKKDGBAO");
        s.group_type = type("", "OLABFALNLJM");
        s.group_members =
            m.Resolve("", "OLABFALNLJM", "get_Members", "System.Collections.Generic.List<RPG.GameCore.GameEntity>", {});
        auto list = ResolveReturnedReferenceList(m, s.group_members, "RPG.GameCore.GameEntity");
        if (!list.count.code)
            throw std::runtime_error("scale follow group collection unavailable");
        s.members.emplace(list);
        s.follow_set = m.Resolve("", "KLKEKKDGBAO", "GBJGAIBJLBB", "System.Void", {"DBENHLHHAJK&"});
        s.follow_initialize = m.Resolve("", "KLKEKKDGBAO", "LMCEDCHJKFF", "System.Void", {});
        s.component_owner =
            m.Resolve("RPG.GameCore", "GameComponentBase", "get_OwnerRef", "RPG.GameCore.GameEntity", {});
        const auto fields = m.ReadFields("", "KLKEKKDGBAO", false);
        s.follow_target = Field(fields, "", "KLKEKKDGBAO", "DPDJLGOIALF", "RPG.GameCore.GameEntity");
        s.follow_model = Field(fields, "", "KLKEKKDGBAO", "EHFENKGDGJD", "RPG.GameCore.CharacterModelComponent");
        s.follow_point = Field(fields, "", "KLKEKKDGBAO", "DMOMBMJBBNP", "UnityEngine.Transform");
        s.follow_multiplier = Field(fields, "", "KLKEKKDGBAO", "AJBAMNPFGIB",
                                    "System.Collections.Generic.Dictionary<System.Single,System.Single>");
        s.follow_late = Field(fields, "", "KLKEKKDGBAO", "IKCMMJAPKPC", "System.Boolean");
        s.follow_outline = Field(fields, "", "KLKEKKDGBAO", "MNMOPFJLDCJ", "System.Boolean");
        const auto input = m.ReadFields("", "DBENHLHHAJK", false);
        if (Field(input, "", "DBENHLHHAJK", "FCKCHPNEEKB", "RPG.GameCore.GameEntity") != 16 ||
            Field(input, "", "DBENHLHHAJK", "LLDLNGNKGKL", "System.String") != 24 ||
            Field(input, "", "DBENHLHHAJK", "LJMFLOELCGF", "System.Boolean") != 32 ||
            Field(input, "", "DBENHLHHAJK", "OANOKPFOGIP", "System.Boolean") != 33 ||
            Field(input, "", "DBENHLHHAJK", "NOOBGJEKLEG",
                  "System.Collections.Generic.Dictionary<System.Single,System.Single>") != 40)
            throw std::runtime_error("scale follow value ABI differs");
        prepare_follow(type("RPG.GameCore", "GameEntity"), s.follow_type);
    } catch (const std::exception& error) {
        s.follow_error = error.what();
    }
    return s;
}
}
