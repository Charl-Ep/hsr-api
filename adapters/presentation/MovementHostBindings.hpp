#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/presentation/movement/AnimationMovementAccess.hpp>
#include <functional>
namespace hksr::api::detail {
struct MovementHostBindings {
    Method component, destroy, initialize, root_get, root_set, callback_get, callback_set, animator_get, is_instance;
    std::size_t external_root = 0, external_move = 0, proxy = 0, thread_data = 0, pending = 0;
    void* movement_type = nullptr;
    void* adventure_type = nullptr;
    void* transform_type = nullptr;
    void* data_type = nullptr;
    void* provider_type = nullptr;
    void* entity_type = nullptr;
    AnimationMovementAccessBinding access;
};
MovementHostBindings ResolveMovementHostBindings(RuntimeMetadata&, const std::function<void*(ObjectResult)>& retain);
}
