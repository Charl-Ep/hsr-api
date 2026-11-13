#pragma once
#include <hksr/animation/ActorBgmSync.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/managed/ManagedArrays.hpp>

namespace hksr::api::detail {
class ActorBgmSyncFactory {
  public:
    ActorBgmSyncFactory(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, ManagedArrays, EntityOwner);
    ~ActorBgmSyncFactory();
    std::unique_ptr<ActorBgmSync> Create(EntityToken, GameWorld*);
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
