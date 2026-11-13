#pragma once
#include "EntityAccess.hpp"
#include "EntityFactory.hpp"
#include "LifecycleHooks.hpp"

namespace hksr::api {
struct NpcCreateInput {
    GameWorld* world = nullptr;
    EntityManager* manager = nullptr;
    MapNpcDefinition* definition = nullptr;
    NpcDataRow* row = nullptr;
    bool factory_option = true;
};
struct StoryCreateInput {
    GameWorld* world = nullptr;
    EntityManager* manager = nullptr;
    std::string unique_name, model_path, adventure_config;
};
struct PartnerCreateInput {
    GameWorld* world = nullptr;
    EntityManager* manager = nullptr;
    EntityToken summoner;
    std::string model_path, character_config;
};
struct EntityReceipt {
    EntityToken token;
    EntitySnapshot observation;
    RetirementResult retirement;
    bool retirement_started = false, disposed = false;
};
struct EntityCreationStatus {
    CreateEntityResult factory;
    CreationReceipt creation;
    std::vector<EntityReceipt> entities;
    std::vector<std::string> errors;
    bool create_started = false, create_returned = false;
    bool removal_requested = false, closed = false;
};
}
