#pragma once
#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <map>
#include <set>
#include <tuple>

namespace hksr::api::detail {
// A lifetime backstop for native event effects. Action-level cleanup remains
// with its producer. Only receipts attributed to SDK-created entities enter.
class EntityEffectRetirement {
  public:
    using Alive = std::function<bool(EntityOrigin)>;
    HKSR_API EntityEffectRetirement(EffectRequestServices, Alive);
    HKSR_API void Before(const EffectSubmission&);
    HKSR_API void After(const EffectSubmission&, bool returned);
    HKSR_API void Collect();
    HKSR_API std::size_t Pending() const;

  private:
    using Key = std::tuple<void*, void*, std::uint32_t>;
    struct Entry {
        Key key;
        EntityOrigin origin;
        std::unique_ptr<OwnedEffectRequest> request;
        bool retained = false;
    };
    EffectRequestServices services_;
    Alive alive_;
    std::map<std::uint64_t, Entry> entries_;
    std::map<Key, std::uint64_t> keys_;
    std::set<std::uint64_t> calls_;
    std::uint64_t next_ = 0;
    bool collecting_ = false;
};
}
