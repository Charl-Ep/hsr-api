#include <hksr/presentation/events/AnimationExitConfig.hpp>
#include "../../../adapters/presentation/AnimationExitSchema.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::api {
struct AnimationExitConfig::State {
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    std::vector<std::string> errors;
    std::string operation = "retain inputs";
    void* groups = nullptr;
    bool attempted = false, closed = false;
    detail::AnimationExitSchema schema;
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, TypeRelationships& t, ReferenceMethods r,
          ArrayMethods a)
        : roots(r), schema(
                        m, d, o, t, ManagedArrays(std::move(a)),
                        [this](ObjectResult value, bool nullable) { return Keep(value, nullable); }, operation) {}
    static void Require(bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
    }
    void* Keep(ObjectResult r, bool nullable = false) {
        if (!r.call.returned)
            throw std::runtime_error("managed call failed; exception=" + std::to_string(r.call.exception));
        Require(r.object || nullable, "animation exit input unavailable");
        if (!r.object || retained.count(r.object))
            return r.object;
        held.push_back(0);
        held.back() = roots.Keep(r.object);
        Require(roots.Status(held.back()).ready, "animation exit retention failed");
        retained.insert(r.object);
        return r.object;
    }
};
AnimationExitConfig::AnimationExitConfig(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, TypeRelationships& t,
                                         ReferenceMethods r, ArrayMethods a)
    : state_(new State(m, d, o, t, r, std::move(a))) {}
AnimationExitConfig::~AnimationExitConfig() {
    delete state_;
}
void* AnimationExitConfig::Groups() const {
    return state_->groups;
}
const std::vector<std::string>& AnimationExitConfig::Errors() const {
    return state_->errors;
}
bool AnimationExitConfig::Prepare(const std::string& name, const std::vector<EffectRemovalSource>& sources) {
    auto& s = *state_;
    if (s.attempted || s.closed)
        return false;
    s.attempted = true;
    try {
        State::Require(!name.empty(), "animation exit state is empty");
        std::vector<void*> tasks;
        for (const auto& source : sources)
            try {
                tasks.push_back(s.schema.Source(source));
            } catch (const std::exception& e) {
                s.errors.push_back(source.ability + ": " + s.operation + ": " + e.what());
            }
        State::Require(!tasks.empty(), "no independently readable animation exit task");
        s.groups = s.schema.Build(name, tasks);
        return true;
    } catch (const std::exception& e) {
        s.errors.push_back(s.operation + ": " + e.what());
        return false;
    }
}
bool AnimationExitConfig::Close() {
    auto& s = *state_;
    for (auto it = s.held.begin(); it != s.held.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.held.erase(it);
        } else
            ++it;
    }
    s.closed = s.held.empty();
    return s.closed;
}
}
