#pragma once
#include "../entity/EntityLifecycle.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hksr::api {
struct ModelScaleMultiplier {
    float height = 0, factor = 1;
};
struct ModelScaleInput {
    float base_scale = 1, outline_scale = 1;
    std::vector<ModelScaleMultiplier> multiplier; // original authored order
    bool equal_ratio_by_min = false;
};
struct ModelScaleSnapshot {
    float factor = 1, outline_scale = 1;
    std::uintptr_t shader_identity = 0;
};
struct ModelScaleTarget {
    EntityOrigin origin;
    std::uintptr_t model_identity = 0, art_identity = 0;
    std::function<bool()> current;
    std::function<ModelScaleSnapshot()> read;
    std::function<void(const ModelScaleInput&)> prepare; // private config only, no model write
    std::function<float(const ModelScaleInput&)> multiplier;
    std::function<void(const ModelScaleInput&, float)> apply;
    std::function<void(ModelScaleSnapshot)> restore;
    std::function<bool()> close;
};
struct ModelScaleProgramServices {
    std::function<std::optional<ModelScaleTarget>(EntityOrigin)> bind;
};
enum class ModelScaleProgramState { Applied, Pending, Unsupported, Failed };
struct ModelScaleProgramResult {
    ModelScaleProgramState state = ModelScaleProgramState::Failed;
    std::size_t targets = 0, applied = 0;
    std::string detail;
};
// Explicit owned targets only. Uses the official per-target scale helper,
// including outline, scale notifications and outfit refresh. No TaskContext,
// target evaluator, task scheduler or battle-target cache is executed.
class ModelScaleProgram {
  public:
    HKSR_API explicit ModelScaleProgram(ModelScaleProgramServices);
    HKSR_API ~ModelScaleProgram(); // no native calls
    HKSR_API ModelScaleProgramResult Set(std::uint64_t scope, const std::vector<EntityOrigin>&, const ModelScaleInput&);
    HKSR_API bool Release(std::uint64_t scope);
    HKSR_API bool ReleaseParticipant(EntityOrigin);
    HKSR_API bool Close();
    HKSR_API const std::string& Error() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
