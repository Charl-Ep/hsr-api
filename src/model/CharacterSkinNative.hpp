#pragma once
#include <hksr/model/CharacterSkin.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <functional>
namespace hksr::api::detail {
class CharacterSkinFactory {
  public:
    static std::shared_ptr<CharacterSkin> Create(CharacterSkinStatus,
                                                 std::function<std::optional<std::string>(const std::string&)>,
                                                 std::function<std::string(const std::string&)>, std::function<bool()>);
    static CharacterSkinCatalog Read(MethodResolver&, ObjectMetadata&, ManagedObjects&, ReferenceMethods, ManagedArrays,
                                     ComponentConfig, std::function<bool()>& cleanup);
    static std::shared_ptr<CharacterSkin> Prepare(MethodResolver&, ObjectMetadata&, ManagedObjects&, ReferenceMethods,
                                                  ManagedArrays, ComponentConfig, std::uint32_t);
};
}
