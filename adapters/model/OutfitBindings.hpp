#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <cstdint>
#include <functional>
namespace hksr::api::detail {
struct OutfitSelection {
    ObjectResult models;
    int gender = 0, base_type = 0; // Official diagnostics, not selection inputs.
    std::vector<std::string> errors;
};
struct OutfitModel {
    std::uint32_t id;
    void* config;
};
enum class OutfitString { Prefab, Material, Mesh };
Method ResolveOutfitApplicability(MethodResolver&);
// Select the given Avatar's declared path using the current account outfit.
// The caller retains every returned managed reference through its own lifetime.
OutfitSelection SelectOutfitModels(MethodResolver&, ObjectMetadata&, std::uint32_t avatar,
                                   const std::function<void*(ObjectResult)>& retain);
Method ResolveOutfitReplace(MethodResolver&);
Method ResolveOutfitSourceModels(MethodResolver&);
Method ResolveOutfitRefresh(MethodResolver&);
ObjectResult ResolveOutfitComponentType(ManagedObjects&);
ReferenceListMethods ResolveOutfitModelList(ObjectMetadata&, void*);
void* ReadOutfitOverride(ObjectMetadata&, void*);
OutfitModel ReadOutfitModel(ObjectMetadata&, void*);
void* ReadOutfitConfigParts(ObjectMetadata&, void*, bool decorations);
void* ReadOutfitString(ObjectMetadata&, void*, OutfitString);
bool ReadOutfitStatic(ObjectMetadata&, void*);
}
