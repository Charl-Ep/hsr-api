#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/entity/EntityFactory.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
struct StoryRuntimeBindings {
    std::size_t component_owner = 0;
    Method outfit_initialize, emotion_initialize;
};
StoryRuntimeBindings ResolveStoryRuntimeBindings(RuntimeMetadata&);
inline CreateEntityResult CallStoryActorFactory(Method factory, EntityManager* manager, void* graph, void* model,
                                                void* name, void* config) {
    CreateEntityResult out;
    out.call = native::Call(factory, &out.entity, manager, 4, graph, model, static_cast<void*>(nullptr), name,
                            static_cast<void*>(nullptr), config, static_cast<void*>(nullptr),
                            static_cast<void*>(nullptr), true, static_cast<void*>(nullptr));
    return out;
}
inline CreateEntityResult CallPartnerFactory(Method factory, EntityManager* manager, GameEntity* summoner, void* model,
                                             void* config) {
    CreateEntityResult out;
    out.call = native::Call(factory, &out.entity, manager, summoner, model, config);
    return out;
}
}
