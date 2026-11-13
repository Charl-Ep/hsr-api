#pragma once
#include "CatalogSchema.hpp"

namespace hksr::api::detail {
// Display-only reads. Reflection boxes TextID so its value ABI and text-map
// representation never leak into consumers or resource-selection contracts.
class CatalogDisplay {
  public:
    explicit CatalogDisplay(CatalogSchema& access) : a_(access), held_(access) {}
    std::uint32_t Role(std::uint32_t avatar) {
        if (!role_.code)
            role_ = Client("AvatarHelper", "GetRoleID", "System.UInt32", {"System.UInt32"});
        std::uint32_t value = 0;
        Require(native::Call(role_, &value, avatar).returned, "official RoleID unavailable");
        return value;
    }
    CharacterGender Gender(std::uint32_t avatar) {
        if (!gender_.code)
            gender_ = Client("AvatarHelper", "GetGender", "RPG.GameCore.GenderType", {"System.UInt32"});
        int value = 0;
        Require(native::Call(gender_, &value, avatar).returned, "official gender unavailable");
        return value == 1 ? CharacterGender::Male : value == 2 ? CharacterGender::Female : CharacterGender::Unspecified;
    }
    std::string Name(std::uint32_t avatar) {
        if (!name_.code)
            name_ = Client("AvatarHelper", "GetAvatarName", "System.String", {"System.UInt32"});
        CatalogAccess::Scope scope(a_);
        return a_.Text(scope.Keep(a_.Call(name_, avatar)));
    }
    std::string NpcName(std::uint32_t npc) {
        CatalogAccess::Scope scope(a_);
        auto* row = scope.Keep(a_.Call(a_.Static("NPCDataExcelTable", "GetData", "RPG.GameCore.NPCDataRow",
                                               {"System.UInt32"}), npc));
        return Localize(row, "DefaultNPCName");
    }
    std::string DefaultSkinName(std::uint32_t avatar) {
        CatalogAccess::Scope scope(a_);
        auto* row = scope.Keep(a_.Call(a_.Static("AvatarExcelTable", "GetData", "RPG.GameCore.AvatarRow",
                                               {"System.UInt32"}), avatar));
        return Localize(row, "AvatarInitialSkinName");
    }
    std::string PathName(std::uint32_t avatar) {
        if (!path_.code)
            path_ = Client("AvatarHelper", "GetAvatarPath", "RPG.AvatarSystem.AvatarPath", {"System.UInt32"});
        CatalogAccess::Scope scope(a_);
        const auto value = a_.Call(path_, avatar);
        Require(value.call.returned, "official AvatarPath unavailable");
        return value.object ? Localize(scope.Keep(value), "<Name>k__BackingField") : std::string{};
    }
    std::string SkinName(std::uint32_t skin) {
        if (!skin_.code)
            skin_ = Client("AvatarSkinHelper", "GetSkinBySkinID", "RPG.AvatarSystem.AvatarSkin", {"System.UInt32"});
        CatalogAccess::Scope scope(a_);
        auto* selected = scope.Keep(a_.Call(skin_, skin));
        return LocalizeProperty(selected, "get_Name");
    }
    std::string Localize(void* object, const char* member) {
        return LocalizeSelected(object, a_.FieldFor(object, member, "RPG.Client.TextID"));
    }
    std::string Localize(void* object, const std::vector<DeclaredField>& fields, const char* member) {
        return LocalizeSelected(object, Field(fields, member, "RPG.Client.TextID"));
    }

  private:
    std::string LocalizeProperty(void* object, const char* getter) {
        Prepare();
        CatalogAccess::Scope scope(a_);
        const auto type = a_.metadata.ReadObjectTypeName(object);
        const auto key = type.name_space + "." + type.name + "." + getter;
        auto found = properties_.find(key);
        if (found == properties_.end()) {
            // The reflected getter boxes its actual TextID result. No guessed
            // hidden return buffer or copied backing-field layout is needed.
            auto* owner = held_.Keep(a_.objects.ManagedType(type.name_space.c_str(), type.name.c_str()));
            auto* name = scope.Keep(a_.objects.NewStringUtf8(getter));
            auto* system_type = scope.Keep(a_.objects.ManagedType("System", "Type"));
            auto* parameters = scope.Keep(a_.arrays.Create(system_type, 0));
            auto* method =
                held_.Keep(a_.Call(a_.methods.Resolve("System", "Type", "GetMethod", "System.Reflection.MethodInfo",
                                                      {"System.String", "System.Type[]"}),
                                   owner, name, parameters));
            found = properties_.emplace(key, method).first;
        }
        auto* boxed = scope.Keep(a_.Call(invoke_, found->second, object, empty_arguments_));
        return LocalizeBoxed(boxed);
    }
    std::string LocalizeSelected(void* object, const DeclaredField& selected) {
        Prepare();
        CatalogAccess::Scope scope(a_);
        const auto key = selected.owner_namespace + "." + selected.owner_name + "." + selected.name;
        auto found = fields_.find(key);
        if (found == fields_.end()) {
            auto* type =
                held_.Keep(a_.objects.ManagedType(selected.owner_namespace.c_str(), selected.owner_name.c_str()));
            auto* name = scope.Keep(a_.objects.NewStringUtf8(selected.name.c_str()));
            auto* field = held_.Keep(
                a_.Call(a_.methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                           {"System.String", "System.Reflection.BindingFlags"}),
                        type, name, 54));
            const auto declaration = a_.metadata.ReadObjectType(field);
            found = fields_
                        .emplace(key, ReflectionField{field, Instance(declaration, "GetValue", "System.Object",
                                                                      {"System.Object"})})
                        .first;
        }
        auto* boxed = scope.Keep(a_.Call(found->second.get, found->second.object, object));
        return LocalizeBoxed(boxed);
    }
    std::string LocalizeBoxed(void* boxed) {
        CatalogAccess::Scope scope(a_);
        // These are the official TextID fields, not a hash of source paths.
        // Cache only within this catalog read/current game language.
        std::optional<std::pair<std::int32_t, std::uint64_t>> identity;
        if (!hash_attempted_) {
            hash_attempted_ = true;
            try {
                const auto fields = a_.metadata.ReadObjectFields(boxed);
                hash_ = Field(fields, "hash", "System.Int32");
                hash64_ = Field(fields, "hash64", "System.UInt64");
                hash_available_ = true;
            } catch (const std::exception&) {
            } // optional optimization, not a name-read gate
        }
        if (hash_available_)
            try {
                identity = std::make_pair(Read<std::int32_t>(boxed, hash_), Read<std::uint64_t>(boxed, hash64_));
                const auto cached = texts_.find(*identity);
                if (cached != texts_.end())
                    return cached->second;
            } catch (const std::exception&) {
            }
        auto* arguments = scope.Keep(a_.arrays.Create(object_type_, 2));
        Require(a_.arrays.Set(arguments, boxed, 0).returned && a_.arrays.Set(arguments, empty_arguments_, 1).returned,
                "localized text argument assignment failed");
        auto text = a_.Text(scope.Keep(a_.Call(invoke_, get_text_, static_cast<void*>(nullptr), arguments)));
        if (identity)
            texts_.emplace(*identity, text);
        return text;
    }
    struct ReflectionField {
        void* object = nullptr;
        Method get;
    };
    CatalogSchema& a_;
    CatalogAccess::Scope held_;
    Method role_, gender_, name_, path_, skin_, invoke_;
    void* object_type_ = nullptr;
    void* empty_arguments_ = nullptr;
    void* get_text_ = nullptr;
    std::map<std::string, ReflectionField> fields_;
    std::map<std::string, void*> properties_;
    bool hash_attempted_ = false, hash_available_ = false;
    DeclaredField hash_, hash64_;
    std::map<std::pair<std::int32_t, std::uint64_t>, std::string> texts_;
    Method Client(const char* owner, const char* name, const char* result, std::initializer_list<const char*> args) {
        return a_.methods.Resolve("RPG.Client", owner, name, result, args, MethodKind::Static);
    }
    void Prepare() {
        if (get_text_)
            return;
        object_type_ = held_.Keep(a_.objects.ManagedType("System", "Object"));
        empty_arguments_ = held_.Keep(a_.arrays.Create(object_type_, 0));
        auto* text_id = held_.Keep(a_.objects.ManagedType("RPG.Client", "TextID"));
        auto* array_type =
            held_.Keep(a_.Call(a_.methods.Resolve("System", "Object", "GetType", "System.Type", {}), empty_arguments_));
        auto* system_type = held_.Keep(a_.objects.ManagedType("System", "Type"));
        auto* types = held_.Keep(a_.arrays.Create(system_type, 2));
        Require(a_.arrays.Set(types, text_id, 0).returned && a_.arrays.Set(types, array_type, 1).returned,
                "localized text signature assignment failed");
        auto* owner = held_.Keep(a_.objects.ManagedType("RPG.Client", "TextmapStatic"));
        auto* name = held_.Keep(a_.objects.NewStringUtf8("GetText"));
        invoke_ = a_.methods.Resolve("System.Reflection", "MethodBase", "Invoke", "System.Object",
                                     {"System.Object", "System.Object[]"});
        get_text_ = held_.Keep(a_.Call(a_.methods.Resolve("System", "Type", "GetMethod", "System.Reflection.MethodInfo",
                                                          {"System.String", "System.Type[]"}),
                                       owner, name, types));
    }
};
}
