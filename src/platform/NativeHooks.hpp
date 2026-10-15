#pragma once
#include <initializer_list>
#include <string>
#include <vector>

namespace hksr::sdk_v1::detail {
struct HookSpec { const char* name; void* target; void* detour; };
// Explicit quiescent install/remove. No destructor patching or global teardown.
class NativeHooks {
public:
    NativeHooks() = default;
    NativeHooks(const NativeHooks&) = delete;
    NativeHooks& operator=(const NativeHooks&) = delete;
    void Install(std::initializer_list<HookSpec> hooks);
    void Remove();
    bool Pending() const;
    void* Original(std::size_t index) const { return hooks_[index].original; }
private:
    struct Hook { std::string name; void* target; void* detour; void* original = nullptr; bool created = false; };
    std::vector<Hook> hooks_;
};
}
