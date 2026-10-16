#pragma once
#include "NpcCreationSession.hpp"
#include "../managed/ManagedObjects.hpp"
#include <functional>

namespace hksr::sdk_v1 {
class RuntimeMetadata;
// The receipt layout is shared with the published NPC API. It describes
// entity ownership, not an NPC row or a character-specific carrier.
using EntityCreationStatus=NpcCreationStatus;
struct StoryCreateInput {
    GameWorld* world=nullptr;
    EntityManager* manager=nullptr;
    std::string unique_name,model_path,adventure_config;
};
// Story creation requests official asynchronous model loading. A returned,
// owned entity is not a loaded model; continue Service and observe readiness.
class HKSR_API EntityCreationSession {
public:
    EntityCreationSession(const EntityOwner&,Method map_factory,Method story_factory,ManagedObjects*,
        EntityAccessMethods,ReferenceMethods,EntityCreationScope::BirthObserver={});
    EntityCreationSession(const EntityOwner& owner,Method map_factory,EntityAccessMethods access,ReferenceMethods refs,EntityCreationScope::BirthObserver observer={})
        :EntityCreationSession(owner,map_factory,{},nullptr,std::move(access),refs,std::move(observer)){}
    ~EntityCreationSession();
    EntityCreationSession(const EntityCreationSession&)=delete;
    EntityCreationSession& operator=(const EntityCreationSession&)=delete;
    void Create(const NpcCreateInput&);
    void Create(const StoryCreateInput&);
    void Remove();
    void Service(const WorldFrame&);
    const EntityCreationStatus& Status() const;
    bool IsStory() const;
private:
    friend class PresentationContext;
    void PrepareStoryRuntime(RuntimeMetadata&);
    void CreateOwned(GameWorld*,EntityManager*,bool require_birth,const std::function<CreateEntityResult()>&);
    struct State;
    State* state_;
};
HKSR_API Method ResolveStoryActorFactory(MethodResolver&);
}
