#pragma once
#include "../../adapters/animation/BgmSyncCalls.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cstddef>
#include <stdexcept>
namespace hksr::api::detail {
struct BgmSyncSlot {
    void* helper = nullptr;
    void* config = nullptr;
};
inline BgmSyncSlot ReadBgmSyncSlot(void* component, std::size_t helper_offset, std::size_t config_offset) {
    BgmSyncSlot slot;
    if (!component ||
        !native::CopyMemoryChecked(&slot.helper, static_cast<char*>(component) + helper_offset, sizeof(void*)))
        throw std::runtime_error("BGM sync helper unreadable");
    if (slot.helper &&
        !native::CopyMemoryChecked(&slot.config, static_cast<char*>(slot.helper) + config_offset, sizeof(void*)))
        throw std::runtime_error("BGM sync config unreadable");
    return slot;
}
inline bool OwnsBgmSyncSlot(BgmSyncSlot slot, void* expected_helper, void* expected_config) {
    return slot.helper && expected_config && slot.config == expected_config &&
           (!expected_helper || slot.helper == expected_helper);
}
struct BgmSyncStopCall {
    CallResult call;
    bool matching = false, active_after = false;
};
inline BgmSyncStopCall StopOwnedBgmSync(Method call, void* component, void* helper, void* config,
                                        std::size_t helper_offset, std::size_t config_offset) {
    const auto before = ReadBgmSyncSlot(component, helper_offset, config_offset);
    if (!OwnsBgmSyncSlot(before, helper, config))
        return {{true, 0}, false, false};
    const auto result = StopBgmSync(call, component);
    return {result, true,
            OwnsBgmSyncSlot(ReadBgmSyncSlot(component, helper_offset, config_offset), before.helper, config)};
}
}
