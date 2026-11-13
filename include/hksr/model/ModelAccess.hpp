#pragma once
#include "../metadata/MethodResolver.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace hksr::api {
struct CharacterModelComponent;
struct DeclaredField;
struct ModelAccessMethods {
    Method art, animator, load_state, changing, root_loaded, art_loaded;
    std::size_t root_offset = 0;
    bool has_root_field = false;
    std::vector<std::string> binding_errors;
};
struct ModelSnapshot {
    Observed<void*> root, art, animator;
    Observed<int> load_state; // raw enum, not a framework readiness policy
    Observed<bool> changing, root_loaded, art_loaded;
};
class ModelAccess {
  public:
    HKSR_API explicit ModelAccess(ModelAccessMethods methods);
    HKSR_API ~ModelAccess();
    HKSR_API ModelAccess(const ModelAccess&) noexcept;
    HKSR_API ModelAccess& operator=(const ModelAccess&) noexcept;
    HKSR_API ModelSnapshot Read(CharacterModelComponent* model) const;
    HKSR_API Observed<void*> Art(CharacterModelComponent* model) const;

  private:
    std::shared_ptr<const ModelAccessMethods> methods_;
};
HKSR_API ModelAccessMethods ResolveModelAccessMethods(MethodResolver& resolver,
                                                      const std::vector<DeclaredField>& fields);
// The adapter normalizes the official model load-state value.
HKSR_API Observed<bool> ModelLoadCompleted(const ModelSnapshot& snapshot);
}
