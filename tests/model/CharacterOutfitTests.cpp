#include <hksr/model/CharacterOutfit.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <Windows.h>
#include <map>
#include <stdexcept>
#include <algorithm>

namespace {
using namespace hksr::api;
int info, type, entity, account, path_object;
using Array = std::vector<void*>;
struct Part {
    bool is_static = false;
    const char* material = "body";
    const char* path = "girl";
} old_part, new_part;
struct Decoration {
    const char* path = "charm";
} old_decoration, new_decoration;
Array old_parts{&old_part}, new_parts{&new_part}, old_decorations{&old_decoration}, new_decorations{&new_decoration};
struct Config {
    Array* parts;
    Array* decorations;
} old_config{&old_parts, &old_decorations}, new_config{&new_parts, &new_decorations};
struct Model {
    unsigned id;
    Config* config;
} old_model{2000, &old_config}, new_model{2200, &new_config};
Array baseline{&old_model}, models{&new_model};
struct Row {
    int base = 4;
} row;
struct Component {
    void* override_value = &baseline;
} component;
std::map<int, void*> roots;
int serial = 0, applications = 0, restorations = 0;
bool fault = false, refresh_fault = false, comparison_fault = false;
int selected_gender = 0, refreshes = 0;
bool mapping_fault = false, models_fault = false, no_path_binding = false;
bool null_path = false, diagnostic_fault = false;
int path_resolutions = 0, path_requests = 0, model_requests = 0;
unsigned selected_avatar = 0;
void* selected_path = nullptr;
void* displayed = &baseline;
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("character outfit test failed");
}
template <class T> Method M(T f) {
    return {reinterpret_cast<void*>(f), &info};
}
int __fastcall Keep(void* p, int, int, void*) {
    roots[++serial] = p;
    return serial;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    roots.erase(id);
}
bool __fastcall Hero(unsigned id, void*) {
    return id == 8005 || id == 8006;
}
int __fastcall Gender(unsigned id, void*) {
    return id == 8005 ? 1 : 2;
}
void* __fastcall Data(unsigned id, void*) {
    Check(id == 8005 || id == 8006);
    return &row;
}
void* __fastcall Account(void*) {
    return &account;
}
void* __fastcall Path(unsigned avatar, void* metadata) {
    Check(metadata == &info && (avatar == 8005 || avatar == 8006));
    ++path_requests;
    selected_avatar = avatar;
    Check(std::any_of(roots.begin(), roots.end(), [](const auto& root) { return root.second == &account; }));
    if (mapping_fault)
        RaiseException(0xE0670003, 0, 0, nullptr);
    return null_path ? nullptr : &path_object;
}
void* __fastcall Models(void* o, int gender, void* path, void* metadata) {
    Check(o == &account && metadata == &info);
    ++model_requests;
    selected_gender = gender;
    selected_path = path;
    Check(path == (null_path ? nullptr : &path_object));
    if (path)
        Check(std::any_of(roots.begin(), roots.end(), [&](const auto& root) { return root.second == path; }));
    if (models_fault)
        RaiseException(0xE0670004, 0, 0, nullptr);
    return &models;
}
void* __fastcall GetComponent(GameEntity* e, void* t, void*) {
    Check(e == reinterpret_cast<GameEntity*>(&entity) && t == &type);
    return &component;
}
void __fastcall Replace(void* c, void* list, bool effect, void*) {
    Check(c == &component && !effect);
    component.override_value = list;
    if (list == &models) {
        ++applications;
        if (fault)
            RaiseException(0xE0670001, 0, 0, nullptr);
        if (old_model.id != new_model.id)
            displayed = list; // official ID-only invalidation
    } else {
        Check(list == &baseline || !list);
        ++restorations;
    }
}
void* __fastcall Source(void* c, void*) {
    Check(c == &component);
    return component.override_value ? component.override_value : &baseline;
}
void __fastcall Refresh(void* c, void*) {
    Check(c == &component);
    ++refreshes;
    if (refresh_fault)
        RaiseException(0xE0670002, 0, 0, nullptr);
    displayed = component.override_value ? component.override_value : &baseline;
}
int __fastcall Count(void* list, void*) {
    return static_cast<int>(static_cast<Array*>(list)->size());
}
void* __fastcall Item(void* list, int index, void*) {
    return static_cast<Array*>(list)->at(index);
}
bool __fastcall Equal(const char* a, const char* b, void*) {
    return a == b || (a && b && std::string(a) == b);
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char* ns, const char* t, const char* n, const char* r, std::initializer_list<const char*> p,
                   MethodKind k) override {
        if (std::string(n) == "GetAvatarPath") {
            ++path_resolutions;
            if (no_path_binding)
                throw std::runtime_error("synthetic path mapping declaration absent");
        }
        MetadataResolver methods(
            {{"RPG.GameCore",
              "CharacterOutfitComponent",
              {{"IsPlayerAvatarID", "System.Boolean", {"System.UInt32"}, true, M(Hero)},
               {"OverrideOutfit",
                "System.Void",
                {"System.Collections.Generic.List<FJEOAPBLMKG>", "System.Boolean"},
                false,
                M(Replace)},
               {"LoadSourceOutfitModels", "System.Collections.Generic.List<FJEOAPBLMKG>", {}, false, M(Source)},
               {"RefreshOutfitParts", "System.Void", {}, false, M(Refresh)}}},
             {"System",
              "Array",
              {{"get_Length", "System.Int32", {}, false, M(Count)},
               {"GetValue", "System.Object", {"System.Int32"}, false, M(Item)}}},
             {"System",
              "String",
              {{"op_Equality", "System.Boolean", {"System.String", "System.String"}, true, M(Equal)}}},
             {"RPG.GameCore",
              "AvatarExcelTable",
              {{"GetData", "RPG.GameCore.AvatarRow", {"System.UInt32"}, true, M(Data)}}},
             {"RPG.Client",
              "AvatarHelper",
              {{"GetGender", "RPG.GameCore.GenderType", {"System.UInt32"}, true, M(Gender)},
               {"GetAvatarPath", "RPG.AvatarSystem.AvatarPath", {"System.UInt32"}, true, M(Path)}}},
             {"RPG.Client",
              "PlayerOutfitHelper",
              {{"GetPlayerOutfit", "RPG.Client.AvatarOutfit", {}, true, M(Account)}}},
             {"RPG.Client",
              "AvatarOutfit",
              {{"GetModels",
                "System.Collections.Generic.List<FJEOAPBLMKG>",
                {"RPG.GameCore.GenderType", "RPG.AvatarSystem.AvatarPath"},
                false,
                M(Models)}}}});
        return methods.Resolve(ns, t, n, r, p, k);
    }
    DeclaredType ReadObjectType(void* object) override {
        if (comparison_fault)
            throw std::runtime_error("test unread list declaration");
        Check(object == &baseline || object == &models);
        return {"System.Collections.Generic",
                "List`1",
                {{"get_Count", "System.Int32", {}, false, M(Count)},
                 {"get_Item", "FJEOAPBLMKG", {"System.Int32"}, false, M(Item)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override {
        if (object == &component)
            return {{"RPG.GameCore", "CharacterOutfitComponent", "_OverrideOutfitModels",
                     "System.Collections.Generic.List<FJEOAPBLMKG>", offsetof(Component, override_value), 1, true}};
        if (object == &old_model || object == &new_model)
            return {{"", "FJEOAPBLMKG", "<ID>k__BackingField", "System.UInt32", offsetof(Model, id), 1, true},
                    {"", "FJEOAPBLMKG", "<Config>k__BackingField", "RPG.GameCore.AvatarOutfitModelConfig",
                     offsetof(Model, config), 1, true}};
        if (object == &old_config || object == &new_config)
            return {{"RPG.GameCore", "AvatarOutfitModelConfig", "PartConfigList",
                     "RPG.GameCore.AvatarOutfitPartConfig[]", offsetof(Config, parts), 6, true},
                    {"RPG.GameCore", "AvatarOutfitModelConfig", "DecorationConfigList",
                     "RPG.GameCore.AvatarOutfitDecorationConfig[]", offsetof(Config, decorations), 6, true}};
        if (object == &old_part || object == &new_part)
            return {{"RPG.GameCore", "AvatarOutfitPartConfig", "IsStatic", "System.Boolean", offsetof(Part, is_static),
                     6, true},
                    {"RPG.GameCore", "AvatarOutfitPartConfig", "MaterialKey", "System.String", offsetof(Part, material),
                     6, true},
                    {"RPG.GameCore", "AvatarOutfitPartConfig", "MeshConfigPath", "System.String", offsetof(Part, path),
                     6, true}};
        if (object == &old_decoration || object == &new_decoration)
            return {{"RPG.GameCore", "AvatarOutfitDecorationConfig", "PrefabConfigPath", "System.String",
                     offsetof(Decoration, path), 6, true}};
        Check(object == &row);
        Check(std::any_of(roots.begin(), roots.end(), [](const auto& root) { return root.second == &row; }));
        if (diagnostic_fault)
            throw std::runtime_error("synthetic base-type diagnostic absent");
        return {{"RPG.GameCore", "AvatarRow", "AvatarBaseType", "RPG.GameCore.AvatarBaseType", offsetof(Row, base), 6,
                 true}};
    }
    ObjectResult ManagedType(const char* ns, const char* n) override {
        Check(std::string(ns) == "RPG.GameCore" && std::string(n) == "CharacterOutfitComponent");
        return {{true, 0}, &type};
    }
    ObjectResult NewStringUtf8(const char*) override {
        throw std::runtime_error("unused string");
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("unused allocation");
    }
};
}
void TestCharacterOutfit() {
    Provider p;
    ReferenceMethods references{M(Keep), M(Target), M(Free), {}, {}};
    EntityAccessMethods entities;
    entities.component = M(GetComponent);
    CharacterOutfit first(p, p, p, references, entities);
    first.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(first.Status().applied && selected_gender == 1 && first.Status().base_type == 4 && selected_avatar == 8005 &&
          selected_path == &path_object && component.override_value == &models);
    Check(first.Close(true) && component.override_value == &baseline && roots.empty() && restorations == 1);
    Check(first.Close(true) && restorations == 1);
    component.override_value = nullptr;
    CharacterOutfit failed(p, p, p, references, entities);
    fault = true;
    failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8006);
    fault = false;
    Check(!failed.Status().applied && failed.Status().apply.exception == 0xE0670001 &&
          component.override_value == &models && selected_gender == 2 && selected_avatar == 8006);
    Check(failed.Close(true) && !component.override_value && roots.empty() && restorations == 2);
    CharacterOutfit disposed(p, p, p, references, entities);
    disposed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(disposed.Close(false) && restorations == 2 && roots.empty()); // never call disposed component
    const auto paths_before_ordinary = path_requests, resolutions_before_ordinary = path_resolutions;
    no_path_binding = true;
    CharacterOutfit ordinary(p, p, p, references, entities);
    ordinary.Prepare(reinterpret_cast<GameEntity*>(&entity), 1001);
    no_path_binding = false;
    Check(!ordinary.Status().applicable && ordinary.Status().errors.empty() && applications == 3 &&
          ordinary.Close(true));
    Check(path_requests == paths_before_ordinary && path_resolutions == resolutions_before_ordinary);
    Check(roots.empty());
    component.override_value = &baseline;
    new_model.id = old_model.id;
    // Distinct config objects with equal content must not force a reload.
    CharacterOutfit unchanged(p, p, p, references, entities);
    unchanged.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(unchanged.Status().applied && !unchanged.Status().config_refresh_required && refreshes == 0);
    Check(unchanged.Close(true) && roots.empty());
    new_part.path = "boy";
    displayed = &baseline;
    CharacterOutfit changed(p, p, p, references, entities);
    changed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(changed.Status().config_refresh_required && changed.Status().refresh.returned && refreshes == 1 &&
          displayed == &models);
    Check(changed.Close(true) && roots.empty() && displayed == &baseline && refreshes == 2);
    Check(changed.Close(true) && refreshes == 2); // restoration does not refresh twice
    new_part.path = "girl";
    new_decoration.path = "other charm";
    CharacterOutfit decoration(p, p, p, references, entities);
    decoration.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(decoration.Status().config_refresh_required && refreshes == 3 && decoration.Close(true) && roots.empty() &&
          displayed == &baseline && refreshes == 4);
    refresh_fault = true;
    CharacterOutfit refresh_failed(p, p, p, references, entities);
    refresh_failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8006);
    refresh_fault = false;
    Check(refresh_failed.Status().applied && refresh_failed.Status().refresh.exception == 0xE0670002 &&
          !refresh_failed.Status().errors.empty());
    const auto failed_prepare_refreshes = refreshes;
    Check(refresh_failed.Close(true) && component.override_value == &baseline && roots.empty() &&
          displayed == &baseline && refreshes == failed_prepare_refreshes + 1);
    // Restoration refresh failure is recorded once, without retrying the write.
    CharacterOutfit restore_refresh_failed(p, p, p, references, entities);
    restore_refresh_failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(restore_refresh_failed.Status().config_refresh_required && displayed == &models);
    const auto before_failed_restore_refresh = refreshes;
    refresh_fault = true;
    Check(restore_refresh_failed.Close(true));
    refresh_fault = false;
    Check(component.override_value == &baseline && roots.empty() && refreshes == before_failed_restore_refresh + 1);
    Check(std::find(restore_refresh_failed.Status().errors.begin(), restore_refresh_failed.Status().errors.end(),
                    "official outfit restoration config refresh failed; not retried") !=
          restore_refresh_failed.Status().errors.end());
    Check(restore_refresh_failed.Close(true) && refreshes == before_failed_restore_refresh + 1);
    // A disposed owner receives neither the restore nor the matching refresh.
    component.override_value = &baseline;
    CharacterOutfit disposed_refresh(p, p, p, references, entities);
    disposed_refresh.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    const auto before_disposed_refresh = refreshes, before_disposed_restore = restorations;
    Check(disposed_refresh.Status().config_refresh_required && disposed_refresh.Close(false) && roots.empty());
    Check(refreshes == before_disposed_refresh && restorations == before_disposed_restore &&
          component.override_value == &models);
    component.override_value = &baseline;
    // Restoring a null override still refreshes the official baseline parts.
    component.override_value = nullptr;
    CharacterOutfit null_baseline(p, p, p, references, entities);
    null_baseline.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(null_baseline.Status().config_refresh_required && displayed == &models);
    Check(null_baseline.Close(true) && !component.override_value && displayed == &baseline && roots.empty());
    component.override_value = &baseline;
    new_decoration.path = old_decoration.path;
    for (int variant = 0; variant < 3; ++variant) {
        new_part = old_part;
        if (variant == 0)
            new_part.material = "different material";
        if (variant == 1)
            new_part.is_static = true;
        if (variant == 2)
            new_parts.push_back(&new_part);
        CharacterOutfit content(p, p, p, references, entities);
        content.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
        Check(content.Status().config_refresh_required && content.Status().refresh.returned);
        Check(content.Close(true) && roots.empty());
        new_parts.resize(1);
    }
    new_part = old_part;
    // Reordering equal IDs/content is not a change; a changed ID delegates to
    // OverrideOutfit even when its config also changed.
    new_model.id = 4000;
    baseline.push_back(&new_model);
    models.insert(models.begin(), &old_model);
    CharacterOutfit reordered(p, p, p, references, entities);
    reordered.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(!reordered.Status().config_refresh_required && reordered.Close(true) && roots.empty());
    baseline.resize(1);
    models = {&new_model};
    new_model.id = 2200;
    new_part.path = "boy";
    const int previous_refreshes = refreshes;
    CharacterOutfit changed_id(p, p, p, references, entities);
    changed_id.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(!changed_id.Status().config_refresh_required && refreshes == previous_refreshes && displayed == &models);
    Check(changed_id.Close(true) && roots.empty());
    comparison_fault = true;
    CharacterOutfit unread(p, p, p, references, entities);
    unread.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    comparison_fault = false;
    Check(unread.Status().applied && !unread.Status().errors.empty() && refreshes == previous_refreshes);
    Check(unread.Close(true) && roots.empty());
    // A null official path is forwarded; base-type diagnostics never select it.
    null_path = true;
    CharacterOutfit zero(p, p, p, references, entities);
    zero.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    Check(zero.Status().applied && zero.Status().base_type == 4 && selected_avatar == 8005 && selected_path == nullptr);
    Check(zero.Close(true) && roots.empty());
    null_path = false;
    // The row is diagnostic only: missing it cannot suppress the official selection.
    diagnostic_fault = true;
    CharacterOutfit diagnostic(p, p, p, references, entities);
    diagnostic.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    diagnostic_fault = false;
    Check(diagnostic.Status().applied && !diagnostic.Status().errors.empty() && selected_avatar == 8005);
    Check(diagnostic.Close(true) && roots.empty());
    const auto applied_before_failure = applications, restored_before_failure = restorations;
    const auto models_before_mapping_failure = model_requests;
    mapping_fault = true;
    CharacterOutfit mapping_failed(p, p, p, references, entities);
    mapping_failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    mapping_fault = false;
    Check(!mapping_failed.Status().applied && !mapping_failed.Status().errors.empty() &&
          model_requests == models_before_mapping_failure);
    Check(applications == applied_before_failure && component.override_value == &baseline);
    Check(mapping_failed.Close(true) && roots.empty() && restorations == restored_before_failure);
    models_fault = true;
    CharacterOutfit selection_failed(p, p, p, references, entities);
    selection_failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    models_fault = false;
    Check(!selection_failed.Status().applied && !selection_failed.Status().errors.empty() &&
          applications == applied_before_failure);
    Check(component.override_value == &baseline && selection_failed.Close(true) && roots.empty() &&
          restorations == restored_before_failure);
    no_path_binding = true;
    CharacterOutfit binding_failed(p, p, p, references, entities);
    binding_failed.Prepare(reinterpret_cast<GameEntity*>(&entity), 8005);
    no_path_binding = false;
    Check(!binding_failed.Status().applied && !binding_failed.Status().errors.empty() &&
          applications == applied_before_failure);
    Check(binding_failed.Close(true) && roots.empty() && component.override_value == &baseline);
}
