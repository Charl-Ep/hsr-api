#pragma once
#include <hksr/model/ActorModelPresentation.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/model/ModelScaleProgram.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>

namespace hksr::api::detail {
class ActorModelPresentationFactory {
  public:
    ActorModelPresentationFactory(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, ManagedArrays,
                                  EntityOwner, std::function<void(EntityToken, GameWorld*)> ensure_face);
    ~ActorModelPresentationFactory();
    std::unique_ptr<ActorModelPresentation> Create(EntityToken, GameWorld*);
    std::unique_ptr<ModelScaleProgram> CreateScaleProgram();
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
