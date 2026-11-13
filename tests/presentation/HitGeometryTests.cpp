#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <map>
#include <functional>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Logic {
    int pad = 9;
    Position3 position{13, -15, 9};
} logic;
int info, anchor, root, position_calls = 0, direction_calls = 0, vector_calls = 0;
Position3 seen_world;
void CheckAt(bool b, int line) {
    if (!b)
        throw std::runtime_error("hit geometry test failed at " + std::to_string(line));
}
#define Check(b) CheckAt((b), __LINE__)
bool Close(float a, float b) { return std::abs(a - b) < 1.e-5f; }
template <class T> Method M(T f) { return {reinterpret_cast<void *>(f), &info}; }
Position3 __fastcall Transform(void *object, Position3 offset, void *metadata) {
    Check(object == &logic && metadata == &info);
    ++vector_calls;
    return {offset.z, offset.y, -offset.x};
}
void __fastcall Position(void *object, Position3 *out, void *metadata) {
    Check(object == &anchor && metadata == &info);
    ++position_calls;
    *out = {100, 10, 20};
}
void __fastcall Direction(void *object, Position3 *in, Position3 *out, void *metadata) {
    Check(object == &anchor && metadata == &info);
    ++direction_calls;
    *out = {in->z, in->y, -in->x};
}
void __fastcall Inverse(void *object, Position3 *in, Position3 *out, void *metadata) {
    Check(object == &root && metadata == &info);
    seen_world = *in;
    *out = {in->x - 100, in->y - 10, in->z - 20};
}
Position3 __fastcall Scale(void *object, void *metadata) {
    Check(object == &logic && metadata == &info);
    return {2, 3, 4};
}
bool missing_anchor = false;
void *__fastcall Attach(void *, void *, bool create, void *metadata) {
    Check(!create && metadata == &info);
    return missing_anchor ? nullptr : &anchor;
}
int entity, model_type, logic_type, animator, other_animator, game_object, attachment_name, next_handle = 0;
int type_calls = 0, component_calls = 0, animator_reads = 0, string_calls = 0;
bool changing_model = false, replaced_animator = false, type_fault = false;
struct Model {
    int pad = 7;
    bool flipped = true;
} model;
std::map<int, void *> handles;
std::function<void()> on_euler, on_component;
int __fastcall Keep(void *object, int, int, void *) {
    handles[++next_handle] = object;
    return next_handle;
}
void *__fastcall Target(int handle, void *) { return handles.at(handle); }
void __fastcall Free(int handle, void *) { Check(handles.erase(handle) == 1); }
void *__fastcall Component(void *object, void *type, void *) {
    Check(object == &entity);
    ++component_calls;
    if (on_component) {
        auto call = std::move(on_component);
        on_component = {};
        call();
    }
    return type == &model_type ? static_cast<void *>(&model) : type == &logic_type ? &logic : nullptr;
}
void *__fastcall Animator(void *object, void *) {
    Check(object == &model);
    ++animator_reads;
    return replaced_animator ? &other_animator : &animator;
}
bool __fastcall Changing(void *object, void *) {
    Check(object == &model);
    return changing_model;
}
void *__fastcall GameObject(void *object, void *) {
    Check(object == &entity);
    return &game_object;
}
void *__fastcall RootTransform(void *object, void *) {
    Check(object == &game_object);
    return &root;
}
Position3 __fastcall Calculate(Position3 relative, float horizontal, float vertical, void *) {
    Check(horizontal == 0 && vertical == 0);
    return {-relative.x, 0, -relative.z};
}
void __fastcall InverseDirection(void *object, Position3 *in, Position3 *out, void *) {
    Check(object == &root);
    *out = *in;
}
void __fastcall Rotation(void *object, Rotation4 *out, void *) {
    Check(object == &root);
    *out = {};
}
void __fastcall Look(Position3 *, Position3 *, Rotation4 *out, void *) { *out = {}; }
void __fastcall Euler(Rotation4 *, Position3 *out, void *) {
    *out = {0, 1.57079632679f, 0};
    if (on_euler) {
        auto call = std::move(on_euler);
        on_euler = {};
        call();
    }
}
struct Objects : ManagedObjects {
    ObjectResult ManagedType(const char *ns, const char *name) override {
        ++type_calls;
        if (type_fault)
            throw std::runtime_error("hit type fixture failure");
        Check(std::string(ns) == "RPG.GameCore");
        const std::string type = name;
        Check(type == "CharacterModelComponent" || type == "TransformComponent");
        return {{true, 0}, type == "CharacterModelComponent" ? &model_type : &logic_type};
    }
    ObjectResult NewStringUtf8(const char *value) override {
        Check(std::string(value) == "HitPoint");
        ++string_calls;
        return {{true, 0}, &attachment_name};
    }
    ObjectResult AllocateObject(const char *, const char *) override {
        throw std::runtime_error("unexpected hit object allocation");
    }
};
} // namespace
void TestHitGeometry() {
    MetadataResolver declarations(
        {{"RPG.GameCore",
          "TransformComponent",
          {{"TransformVector", "RPG.MVector3", {"RPG.MVector3"}, false, M(Transform)},
           {"get_WorldScale", "RPG.MVector3", {}, false, M(Scale)}}},
         {"RPG.GameCore",
          "CharacterModelComponent",
          {{"GetAttachPoint", "UnityEngine.Transform", {"System.String", "System.Boolean"}, false, M(Attach)}}},
         {"UnityEngine",
          "Transform",
          {{"get_position_Injected", "System.Void", {"UnityEngine.Vector3&"}, false, M(Position)},
           {"TransformDirection_Injected",
            "System.Void",
            {"UnityEngine.Vector3&", "UnityEngine.Vector3&"},
            false,
            M(Direction)},
           {"InverseTransformPoint_Injected",
            "System.Void",
            {"UnityEngine.Vector3&", "UnityEngine.Vector3&"},
            false,
            M(Inverse)}}}});
    const std::vector<DeclaredField> fields{
        {"RPG.GameCore", "TransformComponent", "_WorldPos", "RPG.MVector3", offsetof(Logic, position), 0, true}};
    const auto methods = ResolveHitGeometryMethods(declarations, fields);
    Check(methods.binding_errors.empty());
    HitGeometry geometry(methods);
    HitShapeParameters input;
    input.width = .3f;
    input.height = 1.5f;
    input.offset = {1, 2, 3};
    const auto unanchored = geometry.Read(&logic, nullptr, input);
    Check(unanchored.read.returned && !unanchored.value.attached);
    Check(Close(unanchored.value.center.x, 16) && Close(unanchored.value.center.y, -12.25f) &&
          Close(unanchored.value.center.z, 8));
    Check(Close(unanchored.value.vertical_extent, 1.05f) && Close(unanchored.value.effect_radius, .3f));
    Check(position_calls == 0 && direction_calls == 0);
    auto attached = geometry.Read(nullptr, &anchor, input);
    Check(attached.read.returned && attached.value.attached);
    Check(Close(attached.value.center.x, 103) && Close(attached.value.center.y, 12) &&
          Close(attached.value.center.z, 19));
    input.kind = HitShapeKind::Box;
    input.width = 2;
    input.height = 4;
    attached = geometry.Read(nullptr, &anchor, input);
    Check(attached.read.returned && attached.value.vertical_extent == 2 && attached.value.effect_radius == 1);
    auto effect = geometry.EffectOffset(&root, attached.value, {103, 30, 29}, 1, 0);
    Check(effect.read.returned && Close(seen_world.x, 103) && Close(seen_world.z, 20) && Close(seen_world.y, 14));
    Check(Close(effect.value.x, 3) && Close(effect.value.y, 4) && Close(effect.value.z, 0));
    effect = geometry.EffectOffset(&root, attached.value, {103, -30, 29}, 1, 90);
    Check(effect.read.returned && Close(seen_world.x, 104) && Close(seen_world.z, 19) && Close(seen_world.y, 10));
    effect = geometry.EffectOffset(&root, attached.value, {93, 11, 19}, .5f, 0);
    Check(effect.read.returned && Close(seen_world.x, 102) && Close(seen_world.z, 19) && Close(seen_world.y, 11.5f));
    // Coincident XZ uses the source's zero direction and atan2(0,0), not a
    // remembered previous attack or the actor's world rotation.
    effect = geometry.EffectOffset(&root, attached.value, {103, 11, 19}, .5f, 0);
    Check(effect.read.returned && Close(seen_world.x, 103) && Close(seen_world.z, 20));
    auto hit = geometry.EffectPlacement(&logic, &root, attached.value, {103, 11, 29}, 1, 0, {}, false, .3f);
    Check(hit.read.returned && hit.value.direction.x == 0 && hit.value.direction.y == 1 && hit.value.direction.z == 10);
    Check(hit.value.ignore_prefab_attach_point && hit.value.delay == .3f);
    hit = geometry.EffectPlacement(&logic, nullptr, attached.value, {103, 11, 29}, -1, 0, Position3{1, 2, 3}, true, 0);
    Check(hit.read.returned && hit.value.offset.x == 2 && hit.value.offset.y == 6 && hit.value.offset.z == 12 &&
          !hit.value.ignore_prefab_attach_point);
    hit =
        geometry.EffectPlacement(nullptr, nullptr, unanchored.value, {13, -15, 9}, -1, 0, Position3{1, 2, 3}, true, 0);
    Check(hit.read.returned && hit.value.offset.y == 2 &&
          hit.value.direction.y == -15); // bottom is clamped to world zero
    effect = geometry.EffectOffset(nullptr, {}, {}, -1, 0);
    Check(effect.read.returned && effect.value.x == 0 && effect.value.y == 0 && effect.value.z == 0);
    Check(!geometry.EffectOffset(nullptr, attached.value, {}, 0, 0).read.returned);
    input.kind = static_cast<HitShapeKind>(2);
    Check(!geometry.Read(&logic, nullptr, input).read.returned);
    input.kind = HitShapeKind::Capsule;
    input.width = -1;
    Check(!geometry.Read(&logic, nullptr, input).read.returned);
    auto missing = methods;
    missing.position = {};
    input.width = 1;
    Check(!HitGeometry(missing).Read(&logic, &anchor, input).read.returned); // no silent root fallback
    missing = ResolveHitGeometryMethods(declarations, {});
    Check(!HitGeometry(missing).Read(&logic, nullptr, input).read.returned);
    Check(HitGeometry(missing).Read(nullptr, &anchor, input).read.returned); // unused logical field isn't a gate
    const auto saved_position = logic.position;
    logic.position.x = std::numeric_limits<float>::infinity();
    Check(!geometry.Read(&logic, nullptr, input).read.returned);
    logic.position = saved_position;
    const ReferenceMethods references{M(Keep), M(Target), M(Free), {}, {}};
    EntityAccessMethods entities;
    entities.component = M(Component);
    ModelAccessMethods models;
    models.animator = M(Animator);
    models.changing = M(Changing);
    ActorHitGeometryBindings spatial;
    spatial.placement.game_object = M(GameObject);
    spatial.scene.transform = M(RootTransform);
    spatial.geometry = methods;
    spatial.direction.inverse_direction = M(InverseDirection);
    spatial.direction.rotation = M(Rotation);
    spatial.direction.look_rotation = M(Look);
    spatial.direction.to_euler = M(Euler);
    spatial.direction.calculate_world = M(Calculate);
    ResolveHitDirectionModel(spatial.direction, {{"RPG.GameCore", "CharacterModelComponent", "_ModelFlip",
                                                  "System.Boolean", offsetof(Model, flipped), 0, true}});
    Check(spatial.direction.binding_errors.empty());
    auto *owned = reinterpret_cast<GameEntity *>(&entity);
    const auto token = TrackedEntities().Track(owned);
    Objects objects;
    const auto make_session = [&](std::optional<ActorHitGeometryBindings> geometry_bindings) {
        return std::make_unique<ActorAnimationSession>(
            token, references, EntityAccess(entities), ModelAccess(models), AnimationControl(AnimationControlMethods{}),
            ProfileInputs(ProfileInputMethods{}), std::move(geometry_bindings));
    };
    {
        auto session = make_session({});
        Check(!session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).read.returned);
        Check(!session->ReadHitEffectPlacement({}, input, "", -1, 0, {}, false, 0, objects).read.returned);
        Check(type_calls == 0 && component_calls == 0 && string_calls == 0 && handles.empty());
        Check(session->Close());
    }
    {
        auto placement_only = spatial;
        placement_only.direction = {};
        auto session = make_session(std::move(placement_only));
        const auto placement =
            session->ReadHitEffectPlacement({13, 100, 19}, input, "", -1, 0, Position3{1, 2, 3}, true, 0, objects);
        Check(placement.read.returned && placement.value.offset.y == 6 && placement.value.direction.y == 100);
        Check(!session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).read.returned);
        Check(session->Close() && handles.empty());
    }
    {
        auto session = make_session(spatial);
        Check(session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).value == -90);
        Check(handles.size() == 3);
        changing_model = true;
        Check(!session->ReadHitAngle({}, input, "", 0, 0, objects).read.returned);
        changing_model = false;
        on_euler = [] { replaced_animator = true; };
        Check(!session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).read.returned);
        replaced_animator = false;
        const auto anchored = session->ReadHitEffectPlacement({}, input, "HitPoint", -1, 0, {}, false, 0, objects);
        Check(anchored.read.returned && handles.size() == 4);
        session->Resources().Drain();
        Check(handles.size() == 3);
        missing_anchor = true;
        const auto positions = position_calls, vectors = vector_calls;
        Check(session->ReadHitEffectPlacement({}, input, "HitPoint", -1, 0, {}, false, 0, objects).read.returned);
        Check(position_calls == positions && vector_calls == vectors + 1);
        missing_anchor = false;
        session->Resources().Drain();
        int reads_at_close = 0;
        on_euler = [&] {
            Check(session->Close());
            reads_at_close = component_calls;
        };
        Check(!session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).read.returned);
        Check(component_calls == reads_at_close && handles.empty());
        Check(!session->ReadHitAngle({}, input, "", 0, 0, objects).read.returned);
        Check(!session->ReadHitEffectPlacement({}, input, "", -1, 0, {}, false, 0, objects).read.returned);
        Check(component_calls == reads_at_close);
    }
    {
        auto without_anchor = spatial;
        without_anchor.geometry.attach_point = {};
        auto session = make_session(std::move(without_anchor));
        const auto vectors = vector_calls;
        Check(!session->ReadHitEffectPlacement({}, input, "HitPoint", -1, 0, {}, false, 0, objects).read.returned);
        Check(vector_calls == vectors); // failed lookup is not a successful null attachment
        Check(session->Close() && handles.empty());
    }
    {
        auto session = make_session(spatial);
        const auto animator_calls = animator_reads;
        on_component = [&] { Check(!session->Close()); };
        Check(!session->ReadHitEffectPlacement({}, input, "", -1, 0, {}, false, 0, objects).read.returned);
        Check(animator_reads == animator_calls); // close during lookup prevents the next model read
        Check(session->Close() && handles.empty());
    }
    {
        auto session = make_session(spatial);
        type_fault = true;
        bool failed = false;
        try {
            session->ReadHitAngle({}, input, "", 0, 0, objects);
        } catch (const std::runtime_error &) {
            failed = true;
        }
        type_fault = false;
        Check(failed); // active-session lookup errors are not cancellation
        Check(session->Close() && handles.empty());
    }
    {
        // Exercise the same explicit capability assembly used by runtime clients.
        PresentationBindings bindings;
        bindings.references = references;
        bindings.entity = entities;
        bindings.model = models;
        bindings.hit_geometry = spatial.geometry;
        bindings.hit_direction = spatial.direction;
        bindings.placement = spatial.placement;
        bindings.scene = spatial.scene;
        PresentationContext context(objects, std::move(bindings));
        auto session = context.Animations()(token);
        const auto angle = session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects);
        Check(angle.read.returned && angle.value == -90);
        on_euler = [&] { TrackedEntities().Disposed(token); };
        Check(!session->ReadHitAngle({13, 100, 19}, input, "", 0, 0, objects).read.returned);
        Check(session->Close() && context.Close() && handles.empty());
    }
    TrackedEntities().Forget(token);
    const auto next_token = TrackedEntities().Track(owned);
    Check(next_token.generation != token.generation);
    {
        auto stale = make_session(spatial);
        const auto components_before = component_calls;
        Check(!stale->ReadHitAngle({}, input, "", 0, 0, objects).read.returned);
        Check(component_calls == components_before && stale->Close());
    }
    TrackedEntities().Disposed(next_token);
    TrackedEntities().Forget(next_token);
}
