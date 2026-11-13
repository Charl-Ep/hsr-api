#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/animation/ActorEmotion.hpp>
#include <hksr/math/SpatialTypes.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    // Merely reading an unopened host must not start the game or a worker.
    const auto status = hksr::api::ReadRuntimeHostStatus();
    if (status.phase != hksr::api::RuntimeHostPhase::Stopped || status.resident)
        throw std::runtime_error("package consumer unexpectedly started the SDK host");
    hksr::api::ActorEmotionSetup emotion({}, {});
    if (emotion.State().attempted || !emotion.Close())
        throw std::runtime_error("unused actor setup unexpectedly acquired resources");
    const hksr::api::Position3 position{1, 2, 3};
    const hksr::api::Rotation4 rotation;
    hksr::api::ModelAccess models({});
    if (position.z != 3 || rotation.w != 1 || models.Read(nullptr).root.read.returned)
        throw std::runtime_error("installed spatial/model contracts differ");
    // The returned status contains SDK-allocated C++ members and is destroyed
    // in the consumer. Existing boundary tests cover populated results.
    std::cout << hksr::api::build::sdk_version << ' ' << hksr::api::build::compiler << ' '
              << hksr::api::build::configuration << '\n';
}
