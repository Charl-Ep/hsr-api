#include "../../src/animation/ActorAnimationParameters.hpp"
#include "../../adapters/animation/AnimationParameterCalls.hpp"
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <iostream>
#include <limits>

using namespace hksr::api;
using namespace hksr::api::detail;
namespace {
void Check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Recorder {
    std::string method;
    float floating = 0;
    int integer = 0;
    bool boolean = false;
    int calls = 0;
    void* text = nullptr;
};
void __fastcall Floating(Recorder* self, void* text, float value, void*) {
    self->method = "float";
    self->floating = value;
    self->text = text;
    ++self->calls;
}
void __fastcall Integer(Recorder* self, void* text, int value, void*) {
    self->method = "int";
    self->integer = value;
    self->text = text;
    ++self->calls;
}
void __fastcall Boolean(Recorder* self, void* text, bool value, void*) {
    self->method = "bool";
    self->boolean = value;
    self->text = text;
    ++self->calls;
}
void __fastcall Trigger(Recorder* self, void* text, void*) {
    self->method = "trigger";
    self->text = text;
    ++self->calls;
}
void __fastcall Reset(Recorder* self, void* text, void*) {
    self->method = "reset";
    self->text = text;
    ++self->calls;
}
DeclaredType Type(const char* space, const char* name) {
    return {space,
            name,
            {{"SetFloat",
              "System.Void",
              {"System.String", "System.Single"},
              false,
              {reinterpret_cast<void*>(&Floating), nullptr}},
             {"SetInteger",
              "System.Void",
              {"System.String", "System.Int32"},
              false,
              {reinterpret_cast<void*>(&Integer), nullptr}},
             {"SetBool",
              "System.Void",
              {"System.String", "System.Boolean"},
              false,
              {reinterpret_cast<void*>(&Boolean), nullptr}},
             {"SetTrigger", "System.Void", {"System.String"}, false, {reinterpret_cast<void*>(&Trigger), nullptr}},
             {"ResetTrigger", "System.Void", {"System.String"}, false, {reinterpret_cast<void*>(&Reset), nullptr}}}};
}
void TypedNativeCalls() {
    MetadataResolver metadata({Type("RPG.Client", "MockAnimator"), Type("UnityEngine", "Animator")});
    const auto body = ResolveAnimationParameterSetters(metadata, "RPG.Client", "MockAnimator");
    const auto effect = ResolveAnimationParameterSetters(metadata, "UnityEngine", "Animator");
    Recorder mock, animator;
    int name = 0;
    const std::vector<AnimationParameter> values = {{"typed", 1.25f},
                                                    {"typed", 7},
                                                    {"typed", true},
                                                    {"typed", AnimationTrigger{}},
                                                    {"typed", AnimationTrigger{true}}};
    for (const auto& value : values) {
        Check(WriteAnimationParameter(body, &mock, &name, value).returned, "MockAnimator typed call failed");
        Check(WriteAnimationParameter(effect, &animator, &name, value).returned, "effect Animator typed call failed");
    }
    Check(mock.calls == 5 && mock.floating == 1.25f && mock.integer == 7 && mock.boolean && mock.method == "reset" &&
              mock.text == &name,
          "body typed ABI or dispatch differs");
    Check(animator.calls == 5 && animator.integer == 7 && animator.method == "reset",
          "effect typed ABI or dispatch differs");
    Check(!WriteAnimationParameter(body, nullptr, &name, values[0]).returned, "null receiver was called");
}
void SharedExecution() {
    bool current = true, active = true;
    int body_calls = 0, effect_calls = 0, lookups = 0;
    int a = 0, b = 0, c = 0;
    AnimationParameterOperations ops;
    ops.current = [&] { return current; };
    ops.body = [&](const AnimationParameter& p) {
        ++body_calls;
        Check(p.name == "value", "input changed");
        return CallResult{true, 0};
    };
    ops.effects = [&] {
        ++lookups;
        return std::vector<ParameterEffect>{
            {false, true, {&c}, [] { return true; }}, // association/TargetEntity is not ownership
            {true, false, {&c}, [] { return true; }},
            {true, true, {&a, &b}, [&] { return active; }}};
    };
    ops.effect = [&](void* object, const AnimationParameter&) {
        Check(object == &a || object == &b, "foreign effect was written");
        ++effect_calls;
        return CallResult{true, 0};
    };
    auto out = ApplyAnimationParameter(ops, {"value", 3}, AnimationParameterPropagation::BodyOnly);
    Check(out.state == AnimationParameterState::Applied && out.body.returned && lookups == 0,
          "body-only depended on effects");
    out = ApplyAnimationParameter(ops, {"value", false}, AnimationParameterPropagation::SyncPropState);
    Check(out.state == AnimationParameterState::Applied && out.associated_effects == 3 && out.foreign_effects == 1 &&
              out.unsynchronized_effects == 1 && out.effect_writes == 2 && out.owner_current,
          "SyncPropState/all-Animator contract differs");
    Check(body_calls == 2 && effect_calls == 2, "unexpected dispatch count");
    active = false;
    out = ApplyAnimationParameter(ops, {"value", 1.0f}, AnimationParameterPropagation::SyncPropState);
    Check(out.effect_writes == 0 && out.ended_effects == 1, "released/reused request was written");
    active = true;
    ops.effect = [&](void*, const AnimationParameter&) {
        ++effect_calls;
        return CallResult{false, 123};
    };
    out = ApplyAnimationParameter(ops, {"value", AnimationTrigger{}}, AnimationParameterPropagation::SyncPropState);
    Check(out.state == AnimationParameterState::Partial && out.body.returned && out.effect_failures == 2,
          "partial effect failure concealed body result");
    ops.body = [&](const AnimationParameter&) {
        current = false;
        return CallResult{true, 0};
    };
    const auto before = effect_calls;
    out = ApplyAnimationParameter(ops, {"value", 3}, AnimationParameterPropagation::SyncPropState);
    Check(out.state == AnimationParameterState::OwnerChanged && out.body.returned && effect_calls == before,
          "model change during body write propagated");
    current = false;
    out = ApplyAnimationParameter(ops, {"value", 3}, AnimationParameterPropagation::BodyOnly);
    Check(out.state == AnimationParameterState::OwnerChanged && !out.body_attempted, "ended owner reached body setter");
    current = true;
    ops.body = [](const AnimationParameter&) { return CallResult{false, 999}; };
    out = ApplyAnimationParameter(ops, {"value", 3}, AnimationParameterPropagation::SyncPropState);
    Check(out.state == AnimationParameterState::Failed && out.body.exception == 999, "failed native call receipt lost");
    out = ApplyAnimationParameter(ops, {"value", std::numeric_limits<float>::infinity()},
                                  AnimationParameterPropagation::BodyOnly);
    Check(out.state == AnimationParameterState::Unavailable && !out.body_attempted, "nonfinite float accepted");
    out = ApplyAnimationParameter(ops, {std::string("a\0b", 3), 2}, AnimationParameterPropagation::BodyOnly);
    Check(!out.body_attempted, "embedded null accepted");
}
void ReplacementAndMissingEffects() {
    int epoch = 1, writes = 0, a = 0, b = 0;
    AnimationParameterOperations ops{
        [&] { return epoch == 1; }, [](const AnimationParameter&) { return CallResult{true, 0}; },
        [&] { return std::vector<ParameterEffect>{{true, true, {&a, &b}, [] { return true; }}}; },
        [&](void*, const AnimationParameter&) {
            ++writes;
            epoch = 2;
            return CallResult{true, 0};
        }};
    const auto out = ApplyAnimationParameter(ops, {"p", true}, AnimationParameterPropagation::SyncPropState);
    Check(out.state == AnimationParameterState::OwnerChanged && writes == 1, "old model operation adopted a new epoch");
    ops.current = [] { return true; };
    ops.effects = {};
    const auto partial = ApplyAnimationParameter(ops, {"p", 1}, AnimationParameterPropagation::SyncPropState);
    Check(partial.state == AnimationParameterState::Partial && partial.body.returned,
          "unavailable propagation rolled back or hid body write");
}
struct InheritedList {
    void* first;
    void* count_method;
    void* item_method;
};
int __fastcall InheritedCount(InheritedList* self, void* method) {
    return method == self->count_method ? 1 : -1;
}
void* __fastcall InheritedItem(InheritedList* self, int index, void* method) {
    return index == 0 && method == self->item_method ? self->first : nullptr;
}
void InheritedEffectCollections() {
    // FindEffectsByEntity returns MonoEffectList; GetAnimators returns
    // PoolList<Animator>. Neither must redeclare inherited closed-list methods.
    for (const auto* element : {"RPG.Client.MonoEffect", "UnityEngine.Animator"}) {
        int value = 0, count_info = 0, item_info = 0;
        const DeclaredType derived{"RPG.Client", "DerivedCollection", {}};
        const DeclaredType base{
            "System.Collections.Generic",
            "List`1",
            {{"get_Count", "System.Int32", {}, false, {reinterpret_cast<void*>(&InheritedCount), &count_info}},
             {"get_Item", element, {"System.Int32"}, false, {reinterpret_cast<void*>(&InheritedItem), &item_info}}}};
        Check(!ResolveReferenceListMethods(derived, element).count.code,
              "fixture unexpectedly declares inherited method");
        const auto methods = ResolveReferenceListHierarchy(std::vector<DeclaredType>{derived, base}, element);
        Check(methods.binding_errors.empty() && methods.count.metadata == &count_info &&
                  methods.item.metadata == &item_info,
              "closed parent collection MethodInfo lost");
        InheritedList receiver{&value, &count_info, &item_info};
        ManagedReferenceList list(methods);
        const auto count = list.Count(&receiver);
        const auto item = list.Item(&receiver, 0);
        Check(count.read.returned && count.value == 1 && item.call.returned && item.object == &value,
              "inherited effect collection native receiver or generic context differs");
    }
}
}
int main() try {
    TypedNativeCalls();
    SharedExecution();
    ReplacementAndMissingEffects();
    InheritedEffectCollections();
    std::cout << "Typed MockAnimator parameters, inherited SyncPropState collections, ownership and model boundaries "
                 "passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
