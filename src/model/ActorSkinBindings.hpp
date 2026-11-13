#pragma once
#include <hksr/model/CharacterSkin.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/EntityProvenance.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <memory>
#include <functional>

namespace hksr::api {
class RuntimeMetadata;
struct ManagedObjects;
namespace detail {
// Retains prepared skin inputs and associates them with exact owned births.
// Bindings survive retirement callbacks until that entity generation ends.
class ActorSkinBindings {
  public:
    ActorSkinBindings(ManagedObjects&, ReferenceMethods, EntityOwner, ManagedArrays, ComponentConfig);
    ~ActorSkinBindings();
    void Prepare(RuntimeMetadata&);
    CharacterSkinCatalog Read() const;
    std::shared_ptr<CharacterSkin> PrepareSkin(std::uint32_t) const;
    std::shared_ptr<CharacterSkin> Find(GameEntity*, GameWorld* = nullptr) const;
    std::function<void(EntityOrigin)> BirthObserver(std::shared_ptr<CharacterSkin>,
                                                    std::function<void(EntityOrigin)> remember) const;
    void Collect();
    bool ActorsDrained() const;
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
}
