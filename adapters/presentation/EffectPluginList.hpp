#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
namespace hksr::api::detail {
inline ReferenceListMethods ResolveEffectPluginList(RuntimeMetadata& metadata, void* list) {
    return ResolveReferenceListHierarchy(metadata, metadata.ObjectType(list), "RPG.Client.MonoEffectPluginBase");
}
}
