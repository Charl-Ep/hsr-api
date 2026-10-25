#include <hksr/entity/EntityAccess.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
int root, art, animator, metadata;
struct Model { unsigned char prefix[48]{}; void* root = &::root; } model;
bool fail_art = false;
void Check(bool ok) { if (!ok) throw std::runtime_error("model observation test failed"); }
void CheckSelf(void* self, void* info) { Check(self == &model && info == &metadata); }
void* __fastcall Art(void* self, void* info) { CheckSelf(self, info); if (fail_art) RaiseException(0xE0520001, 0, 0, nullptr); return &art; }
void* __fastcall Animator(void* self, void* info) { CheckSelf(self, info); return &animator; }
int __fastcall LoadState(void* self, void* info) { CheckSelf(self, info); return 5; }
bool __fastcall False(void* self, void* info) { CheckSelf(self, info); return false; }
bool __fastcall True(void* self, void* info) { CheckSelf(self, info); return true; }
template<class T> Method M(T f) { return {reinterpret_cast<void*>(f), &metadata}; }
}
void TestEntityAccess() {
    MetadataResolver resolver({{"RPG.GameCore", "CharacterModelComponent", {
        {"GetModelArt", "UnityEngine.GameObject", {}, false, M(Art)},
        {"get_Animator", "UnityEngine.Animator", {}, false, M(Animator)},
        {"get_LoadState", "RPG.GameCore.EntityLoadState", {}, false, M(LoadState)},
        {"get_IsChangingModel", "System.Boolean", {}, false, M(False)},
        {"IsModelLoad", "System.Boolean", {}, false, M(True)},
        {"IsArtLoaded", "System.Boolean", {}, false, M(False)}}}});
    const std::vector<DeclaredField> fields{{"RPG.GameCore", "CharacterModelComponent", "_unityGO", "UnityEngine.GameObject", offsetof(Model, root)}};
    auto methods = ResolveModelAccessMethods(resolver, fields);
    Check(methods.binding_errors.empty() && methods.root_offset == 48);
    ModelAccess reader(methods);
    auto snapshot = reader.Read(reinterpret_cast<CharacterModelComponent*>(&model));
    Check(snapshot.root.value == &root && snapshot.art.value == &art && snapshot.animator.value == &animator);
    Check(snapshot.root_loaded.value && !snapshot.art_loaded.value && snapshot.load_state.value == 5);
    fail_art = true;
    snapshot = reader.Read(reinterpret_cast<CharacterModelComponent*>(&model));
    Check(snapshot.art.read.exception == 0xE0520001 && snapshot.animator.read.returned && snapshot.root.read.returned);
    fail_art = false;
    // Unknown field is reported independently; other getters still execute.
    methods = ResolveModelAccessMethods(resolver, {});
    Check(!methods.has_root_field && methods.binding_errors.size() == 1);
    snapshot = ModelAccess(methods).Read(reinterpret_cast<CharacterModelComponent*>(&model));
    Check(!snapshot.root.read.returned && snapshot.art.value == &art);
    MetadataResolver empty({});
    const auto absent = ResolveEntityAccessMethods(empty);
    Check(absent.binding_errors.size() == 8);
    MetadataResolver entity_types({{"RPG.GameCore","GameEntity",{{"get_EntityType","RPG.GameCore.EntityType",{},false,M(LoadState)}}}});
    const auto typed=ResolveEntityAccessMethods(entity_types);
    Check(EntityAccess(typed).Type(reinterpret_cast<GameEntity*>(&model)).value==5);
    Check(!EntityAccess(typed).Type(nullptr).read.returned);
    const auto retirement = EntityAccess(absent).RequestRetirement(reinterpret_cast<GameWorld*>(1), reinterpret_cast<GameEntity*>(2));
    Check(!retirement.request_attempted && !retirement.set_immediate.returned);
    std::cout << "Entity/model boundary tests passed: independent results and distinct root/Art/Animator.\n";
}
