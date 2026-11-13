#include <hksr/model/ActorModelPresentation.hpp>
#include <hksr/model/ModelScaleProgram.hpp>
#include "../../adapters/model/ModelPresentationCalls.hpp"
#include <iostream>
#include <limits>
#include <map>

using namespace hksr::api;
namespace {
void Check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct NativeState {
    FaceMaterialTransition face;
    float scale = 1;
    bool immediate = false;
    void* key = nullptr;
    detail::ModelScaleFollowInput follow;
    unsigned calls = 0;
};
void __fastcall Face(NativeState* s, float cheek, float shy, float shadow, float duration, void*) {
    s->face = {{cheek, shy, shadow}, duration};
    ++s->calls;
}
void __fastcall Factor(NativeState* s, void* key, float value, bool immediate, void*) {
    s->key = key;
    s->scale = value;
    s->immediate = immediate;
    ++s->calls;
}
void __fastcall Follow(NativeState* s, detail::ModelScaleFollowInput* value, void*) {
    s->follow = *value;
    ++s->calls;
}
struct HelperState {
    void* context = reinterpret_cast<void*>(1);
    void* config = nullptr;
    GameEntity* target = nullptr;
    float base = 0, ratio = 0;
    unsigned calls = 0;
};
void __fastcall Construct(HelperState* s, void* context, void* config, void*) {
    s->context = context;
    s->config = config;
    ++s->calls;
}
float __fastcall Ratio(HelperState* s, GameEntity* target, void*) {
    s->target = target;
    ++s->calls;
    return .6f;
}
void __fastcall Apply(HelperState* s, GameEntity* target, float base, float ratio, void*) {
    s->target = target;
    s->base = base;
    s->ratio = ratio;
    ++s->calls;
}
template <class T> Method M(T call) {
    return {reinterpret_cast<void*>(call), nullptr};
}
template <class F> void Reject(F operation) {
    bool failed = false;
    try {
        operation();
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed, "operation was not rejected");
}
void NativeCalls() {
    NativeState s;
    int key = 0, target = 0;
    Check(detail::WriteFaceMaterial(M(Face), &s, {{.1f, .2f, .3f}, .25f}).returned, "face call failed");
    Check(s.face.value == FaceMaterialValue{.1f, .2f, .3f} && s.face.duration == .25f, "face float ABI/order differs");
    Check(detail::WriteModelScaleFactor(M(Factor), &s, &key, .6f).returned && s.key == &key && s.scale == .6f &&
              s.immediate,
          "scale factor ABI differs");
    Check(detail::WriteModelScaleFollow(M(Follow), &s, &target).returned && s.follow.target == &target &&
              !s.follow.point && !s.follow.late && !s.follow.outline && !s.follow.multiplier,
          "plain follow by-ref ABI differs");
    Check(detail::WriteModelScaleFollow(M(Follow), &s, nullptr).returned && !s.follow.target,
          "follow stop is not empty target");
    HelperState h;
    float ratio = 0;
    auto* entity = reinterpret_cast<GameEntity*>(&target);
    Check(detail::ConstructModelScaleHelper(M(Construct), &h, &key).returned && !h.context && h.config == &key,
          "private helper ctor passed live TaskContext or wrong config");
    Check(detail::ReadModelScaleMultiplier(M(Ratio), &ratio, &h, entity).returned && ratio == .6f && h.target == entity,
          "official multiplier ABI differs");
    Check(detail::ApplyModelScaleHelper(M(Apply), &h, entity, .5f, ratio).returned && h.base == .5f && h.ratio == .6f &&
              h.calls == 3,
          "official per-target helper float ABI/order differs");
}
void PublicCalls() {
    bool current = true;
    unsigned writes = 0, closes = 0;
    int entity = 0, world = 0;
    const EntityOrigin target{{reinterpret_cast<GameEntity*>(&entity), 9}, reinterpret_cast<GameWorld*>(&world)};
    EntityOrigin followed;
    FaceMaterialValue face;
    float scale = 1;
    ActorModelPresentationServices services;
    services.current = [&] { return current; };
    services.face = [&] { return face; };
    services.set_face = [&](auto value) {
        face = value.value;
        ++writes;
    };
    services.set_scale_factor = [&](const auto& name, float value) {
        Check(name == "owned-key", "scale key changed");
        scale = value;
        ++writes;
    };
    services.root_scale = [&] { return scale; };
    services.follow_target = [&] { return followed; };
    services.target_current = [&](auto value) {
        return current && value.token.generation == 9 && value.token.entity == target.token.entity &&
               value.world == target.world;
    };
    services.set_follow = [&](auto value) {
        followed = value;
        ++writes;
    };
    services.close = [&] {
        ++closes;
        return true;
    };
    services.model_identity = 4;
    ActorModelPresentation value(std::move(services));
    value.SetFace({{.2f, .4f, .6f}, .2f});
    value.SetScaleFactor("owned-key", -1);
    value.Follow(target);
    Check(value.ModelIdentity() == 4 && value.Face() == face && value.RootScale() == -1 &&
              value.FollowTarget().token.entity == target.token.entity && writes == 3,
          "thin forwarding changed values");
    Reject([&] { value.SetScaleFactor("owned-key", std::numeric_limits<float>::infinity()); });
    Reject([&] { value.SetFace({{}, -1}); });
    auto stale = target;
    stale.token.generation = 10;
    Reject([&] { value.Follow(stale); });
    Check(writes == 3, "invalid inputs reached provider");
    current = false;
    Reject([&] { value.SetFace({{}, 0}); });
    Reject([&] { value.SetScaleFactor("owned-key", 1); });
    Reject([&] { value.Follow({}); });
    Check(value.Close() && value.Close() && closes == 1 && writes == 3, "retired model was written or closed twice");
}
EntityOrigin Origin(std::uint64_t id) {
    return {{reinterpret_cast<GameEntity*>(id * 16), id}, reinterpret_cast<GameWorld*>(1)};
}
struct ScaleFixture {
    struct Actor {
        ModelScaleSnapshot value{1, 1, 4};
        float multiplier = 1;
        bool live = true, ready = true, unsupported = false, fail_after = false;
        unsigned writes = 0, closed = 0, prepared = 0;
    };
    std::map<std::uint64_t, Actor> actors;
    std::function<void(std::uint64_t)> after;
    ModelScaleProgramServices Services() {
        return {[this](EntityOrigin origin) -> std::optional<ModelScaleTarget> {
            auto& a = actors.at(origin.token.generation);
            if (!a.ready)
                return {};
            const auto id = origin.token.generation;
            return ModelScaleTarget{origin,
                                    10,
                                    20,
                                    [&a] { return a.live; },
                                    [&a] {
                                        if (a.unsupported)
                                            throw ModelPresentationUnsupported("nonempty group");
                                        return a.value;
                                    },
                                    [&a](const auto&) {
                                        ++a.prepared;
                                        if (a.unsupported)
                                            throw ModelPresentationUnsupported("nonempty group");
                                    },
                                    [&a](const auto&) { return a.multiplier; },
                                    [this, &a, id](const auto& input, float ratio) {
                                        if (a.unsupported)
                                            throw ModelPresentationUnsupported("nonempty group");
                                        a.value.factor =
                                            input.base_scale * (input.equal_ratio_by_min ? ratio : a.multiplier);
                                        a.value.outline_scale = input.outline_scale;
                                        ++a.writes;
                                        if (after)
                                            after(id);
                                        if (a.fail_after)
                                            throw std::runtime_error("partial helper failure");
                                    },
                                    [&a](auto value) {
                                        a.value = value;
                                        ++a.writes;
                                    },
                                    [&a] {
                                        ++a.closed;
                                        return true;
                                    }};
        }};
    }
};
void CharacterScale() {
    ScaleFixture f;
    f.actors[1];
    f.actors[2];
    ModelScaleProgram p(f.Services());
    ModelScaleInput input;
    input.base_scale = .5f;
    input.outline_scale = .3f;
    f.actors[2].ready = false;
    Check(p.Set(1, {Origin(1), Origin(2)}, input).state == ModelScaleProgramState::Pending && f.actors[1].writes == 0,
          "batch wrote before every target was ready");
    f.actors[2].ready = true;
    f.actors[2].unsupported = true;
    Check(p.Set(1, {Origin(1), Origin(2)}, input).state == ModelScaleProgramState::Unsupported &&
              f.actors[1].writes == 0,
          "batch ignored second group preflight");
    f.actors[2].unsupported = false;
    f.actors[1].multiplier = 2;
    f.actors[2].multiplier = 3;
    input.equal_ratio_by_min = true;
    auto result = p.Set(1, {Origin(1), Origin(2), Origin(1)}, input);
    Check(result.state == ModelScaleProgramState::Applied && result.targets == 2 && result.applied == 2 &&
              f.actors[1].value.factor == .5f && f.actors[2].value.factor == .5f,
          "shared minimum is not official min(1,ratios), or duplicate target applied");
    input.base_scale = .8f;
    Check(p.Set(2, {Origin(1)}, input).state == ModelScaleProgramState::Applied, "overlap did not apply");
    Check(p.Release(1) && f.actors[1].value.factor == .8f && f.actors[2].value.factor == 1,
          "older withdrawal displaced newer scale");
    f.actors[1].value.outline_scale = .9f;
    Check(p.Release(2) && f.actors[1].value.factor == 1 && f.actors[1].value.outline_scale == .9f,
          "cleanup overwrote independent outline");
    Check(p.Close(), "scale program not drained");
    ScaleFixture partial;
    partial.actors[1];
    partial.actors[2];
    ModelScaleProgram q(partial.Services());
    partial.after = [&](auto id) {
        if (id == 1)
            partial.actors[2].unsupported = true;
    };
    result = q.Set(3, {Origin(1), Origin(2)}, input);
    Check(result.state == ModelScaleProgramState::Failed && result.applied == 1 &&
              partial.actors[1].value.factor == .8f,
          "partial batch was downgraded to unsupported or lost write");
    partial.after = {};
    partial.actors[2].unsupported = false;
    Check(q.Release(3) && partial.actors[1].value.factor == 1, "partial write receipt not restored");
    Check(q.Close(), "partial batch roots not drained");
    ScaleFixture replaced;
    replaced.actors[1];
    ModelScaleProgram r(replaced.Services());
    Check(r.Set(4, {Origin(1)}, input).state == ModelScaleProgramState::Applied, "retirement setup failed");
    replaced.actors[1].live = false;
    const auto writes = replaced.actors[1].writes;
    Check(r.Close() && replaced.actors[1].writes == writes, "retired model was restored into new model");
    ScaleFixture waiting;
    waiting.actors[1];
    waiting.actors[2].ready = false;
    ModelScaleProgram pending(waiting.Services());
    Check(pending.Set(5, {Origin(1), Origin(2)}, input).state == ModelScaleProgramState::Pending &&
              pending.Release(5) && pending.Close() && waiting.actors[1].closed == 1 && waiting.actors[1].writes == 0,
          "cancelled preparation leaked roots or wrote a model");
    ScaleFixture native_failure;
    native_failure.actors[1].fail_after = true;
    ModelScaleProgram failed(native_failure.Services());
    Check(failed.Set(6, {Origin(1)}, input).state == ModelScaleProgramState::Failed,
          "native partial failure became skipped");
    native_failure.actors[1].fail_after = false;
    Check(failed.Release(6) && native_failure.actors[1].value.factor == 1 && failed.Close(),
          "failed native write lost cleanup receipt");
}
}
int main() try {
    NativeCalls();
    PublicCalls();
    CharacterScale();
    std::cout << "Model presentation typed calls, actor-bound forwarding and character-scale batches passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
