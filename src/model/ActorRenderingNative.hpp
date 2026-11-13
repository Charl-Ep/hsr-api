#pragma once
#include <hksr/model/ActorRendering.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include "../../adapters/model/RenderingBindings.hpp"
namespace hksr::api::detail {
class ActorRenderingFactory {
  public:
    ActorRenderingFactory(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, ManagedArrays, EntityOwner);
    ~ActorRenderingFactory();
    std::unique_ptr<ActorRendering> Create(EntityToken, GameWorld*);
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
