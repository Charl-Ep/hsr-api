#pragma once
#include <hksr/presentation/movement/OwnedMovementHost.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedArrays.hpp>

namespace hksr::sdk_v1::detail {
// One prepared reflection factory per presentation context, not per character.
class MovementHostFactory {
public:
    MovementHostFactory(RuntimeMetadata&,ReferenceMethods,ManagedArrays,std::function<void(const std::string&)> trace={});
    ~MovementHostFactory();
    std::shared_ptr<OwnedMovementHost> Acquire(EntityToken,void* animator,
        std::function<bool()> current,std::function<bool()> alive);
    void Collect();
    bool Close();
private:
    struct State;std::shared_ptr<State> state_;
};
}
