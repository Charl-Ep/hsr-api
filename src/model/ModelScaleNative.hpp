#pragma once
#include <hksr/model/ModelScaleProgram.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>

namespace hksr::api::detail {
class ModelScaleNative {
  public:
    ModelScaleNative(RuntimeMetadata&, ReferenceMethods);
    ~ModelScaleNative();
    ModelScaleTarget Bind(EntityOrigin, void* model, void* art, std::function<bool()> current,
                          std::function<void()> check_group, std::function<bool()> release_owner);
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
