#pragma once
#include <hksr/model/ActorModelChange.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <optional>
namespace hksr::api::detail {
class ActorModelChangeFactory {
  public:
    HKSR_API ActorModelChangeFactory(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, EntityOwner);
    HKSR_API ActorModelChangeFactory(MethodResolver&, ManagedObjects&, ReferenceMethods, EntityAccess, ModelAccess,
                                     EntityOwner, std::function<bool(EntityToken, GameWorld*)> owns);
    HKSR_API ~ActorModelChangeFactory();
    HKSR_API std::unique_ptr<ActorModelChange> Create(EntityToken, GameWorld*, std::optional<std::string>);
    HKSR_API bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
