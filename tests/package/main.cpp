#include <hksr/runtime/RuntimeHost.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    // Merely reading an unopened host must not start the game or a worker.
    const auto status = hksr::api::ReadRuntimeHostStatus();
    if (status.phase != hksr::api::RuntimeHostPhase::Stopped || status.resident)
        throw std::runtime_error("package consumer unexpectedly started the SDK host");
    // The returned status contains SDK-allocated C++ members and is destroyed
    // in the consumer. Existing boundary tests cover populated results.
    std::cout << hksr::api::build::sdk_version << ' '
              << hksr::api::build::compiler << ' '
              << hksr::api::build::configuration << " target-game="
              << hksr::api::build::target_game_major << '.' << hksr::api::build::target_game_minor << '\n';
}
