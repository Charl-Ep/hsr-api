#include "TimingBindings.hpp"
#include <optional>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
std::size_t Offset(const TimingFields& fields, const char* ns, const char* owner, const char* name, const char* type) {
    std::optional<std::size_t> result;
    for (const auto& field : fields(ns, owner))
        if (field.name == name) {
            if (result || field.type != type || !field.flags_observed || (field.flags & 16))
                throw std::runtime_error("actor timing field differs");
            result = field.offset;
        }
    if (!result)
        throw std::runtime_error(std::string("actor timing field absent: ") + name);
    return *result;
}
template <class F> void Bind(std::string& error, F&& bind) {
    try {
        bind();
    } catch (const std::exception& failure) {
        error = failure.what();
    }
}
}
TimingBindings ResolveTimingBindings(MethodResolver& metadata, const TimingFields& fields, const TimingType& type) {
    TimingBindings s;
    Bind(s.animator_error, [&] {
        s.model_type = type("RPG.GameCore", "CharacterModelComponent");
        s.controller = metadata.Resolve("UnityEngine", "Animator", "get_runtimeAnimatorController",
                                        "UnityEngine.RuntimeAnimatorController", {});
    });
    Bind(s.layer_error, [&] {
        s.layer_index = metadata.Resolve("UnityEngine", "Animator", "GetLayerIndex", "System.Int32", {"System.String"});
        s.layer_weight =
            metadata.Resolve("UnityEngine", "Animator", "GetLayerWeight", "System.Single", {"System.Int32"});
    });
    Bind(s.battle_error, [&] {
        s.battle_type = type("", "CFFPEAALCPH");
        s.battle_animator = metadata.Resolve("", "CFFPEAALCPH", "GetUnityAnimator", "UnityEngine.Animator", {});
    });
    Bind(s.adventure_error, [&] {
        s.adventure_type = type("", "OKMFNNDDCGA");
        s.adventure_mock = metadata.Resolve("", "OKMFNNDDCGA", "FBHJPDAEDGI", "RPG.Client.MockAnimator", {});
        s.mock_animator = metadata.Resolve("RPG.Client", "MockAnimator", "get_Animator", "UnityEngine.Animator", {});
    });
    Bind(s.battle_tween_error, [&] {
        s.battle_tween = metadata.Resolve("", "CFFPEAALCPH", "AGLJAHOBLIO", "System.Void",
                                          {"System.String", "System.Single", "System.Single"});
    });
    Bind(s.adventure_tween_error, [&] {
        s.adventure_tween = metadata.Resolve("", "OKMFNNDDCGA", "AGLJAHOBLIO", "System.Void",
                                             {"System.String", "System.Single", "System.Single"});
    });
    Bind(s.speed_error, [&] { s.speed_offset = Offset(fields, "", "CFFPEAALCPH", "GIFCMBFPFMB", "System.Single"); });
    // A clock start requires its paired observation/removal contract, never an animation receiver.
    Bind(s.clock_error, [&] {
        s.clock_offset = Offset(fields, "RPG.GameCore", "GameEntity", "TimeScaleStack", "RPG.GameCore.TimeScaleStack");
        s.remove = metadata.Resolve("RPG.GameCore", "TimeScaleStack", "RemoveScaleByIndex", "System.Void",
                                    {"System.Int32", "System.Boolean"});
        s.contains =
            metadata.Resolve("RPG.GameCore", "TimeScaleStack", "ContainsIndex", "System.Boolean", {"System.Int32"});
    });
    Bind(s.infinite_error, [&] {
        s.scale_infinite = metadata.Resolve("RPG.GameCore", "TimeScaleStack", "ScaleInfinitely", "System.Int32",
                                            {"System.Single", "System.String", "System.Single", "System.Single",
                                             "UnityEngine.AnimationCurve", "UnityEngine.AnimationCurve",
                                             "System.Single", "System.Boolean"});
    });
    Bind(s.finite_error, [&] {
        s.scale_finite = metadata.Resolve("RPG.GameCore", "TimeScaleStack", "ScaleByDuration", "System.Int32",
                                          {"System.Single", "System.Single", "System.String", "System.Single",
                                           "System.Single", "UnityEngine.AnimationCurve", "UnityEngine.AnimationCurve",
                                           "System.Single", "System.Boolean"});
    });
    return s;
}
}
