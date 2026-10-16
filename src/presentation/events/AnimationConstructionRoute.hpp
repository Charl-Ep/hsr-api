#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <functional>
#include <memory>
#include <cstdint>

namespace hksr::sdk_v1::detail {
// One process entry. Projections run before observations regardless of client
// registration order. Tokens keep each participant alive through native SEH.
struct AnimationConstructionArguments {
    void *receiver=nullptr,*events=nullptr,*behaviors=nullptr,*groups=nullptr,
        *logic=nullptr,*context=nullptr,*registration=nullptr,*file=nullptr,*metadata=nullptr;
    std::uint64_t serial=0;
};
// Non-null only in the original construction, not in policy/observer code.
const AnimationConstructionArguments* ExchangeAnimationConstruction(const AnimationConstructionArguments*) noexcept;
const AnimationConstructionArguments* CurrentAnimationConstruction() noexcept;
struct AnimationConstructionParticipant {
    using Begin=std::function<void*(AnimationConstructionArguments&)>;
    using Finish=std::function<void(void*,bool)>;
    Begin begin;
    Finish finish;
};
class AnimationConstructionSubscription {
public:
    AnimationConstructionSubscription(MethodResolver&,bool projection,AnimationConstructionParticipant);
    ~AnimationConstructionSubscription();
    void Stop() noexcept;
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
