#pragma once
#include <hksr/animation/ActorTiming.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
namespace hksr::api::detail {
class ActorTimingFactory {
  public:
    using Fields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
    HKSR_API ActorTimingFactory(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, EntityOwner);
    HKSR_API ActorTimingFactory(MethodResolver&, ManagedObjects&, Fields, ReferenceMethods, EntityAccess, ModelAccess,
                                EntityOwner, std::function<bool(EntityToken, GameWorld*)> owns);
    HKSR_API ~ActorTimingFactory();
    HKSR_API std::unique_ptr<ActorTiming> Create(EntityToken, GameWorld*);
    HKSR_API bool Close();

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
