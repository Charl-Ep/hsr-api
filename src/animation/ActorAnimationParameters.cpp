#include "ActorAnimationParameters.hpp"
#include <hksr/model/ModelAccess.hpp>
#include "../../adapters/animation/AnimationParameterBindings.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../runtime/RuntimeResident.hpp"

namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("animation propagation field unreadable");
    return value;
}
bool SameOwner(EntityOrigin origin, EntityToken token, GameWorld* world) {
    return origin.token.entity == token.entity && origin.token.generation == token.generation && origin.world == world;
}
}
struct ActorAnimationParameters::State : AnimationParameterBindings {
    std::shared_ptr<RuntimeMetadata> metadata;
    EntityAccess entities;
    ModelAccess models;
    EntityOwner owner;
    ManagedRoots roots;
    std::vector<ManagedRootId> temporary, types;
    bool closed = false, busy = false, effects_prepared = false;
    std::string effect_error;
    struct Binding {
        void* model = nullptr;
        void* art = nullptr;
        void* animator = nullptr;
        void* mock = nullptr;
    };
    State(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess e, ModelAccess models_, EntityOwner o)
        : metadata(RetainRuntimeMetadata(m)), entities(std::move(e)), models(std::move(models_)), owner(std::move(o)),
          roots(refs) {}
    void Keep(void* object, std::vector<ManagedRootId>& handles) {
        if (!object)
            return;
        handles.push_back(roots.Keep(object));
        if (!roots.Status(handles.back()).ready)
            throw std::runtime_error("animation parameter retention failed");
    }
    bool Release(std::vector<ManagedRootId>& handles) {
        for (auto it = handles.begin(); it != handles.end();) {
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = handles.erase(it);
            } else
                ++it;
        }
        return handles.empty();
    }
    void* Require(ObjectResult result, const char* message) {
        if (!result.call.returned || !result.object)
            throw std::runtime_error(message);
        Keep(result.object, temporary);
        return result.object;
    }
    bool Owned(EntityToken token, GameWorld* world) const {
        if (closed || !world || !token.entity || !token.generation || TrackedEntities().IsDisposed(token) ||
            TrackedEntities().IsRetiring(token) || owner.OwnedToken(token.entity).generation != token.generation)
            return false;
        const auto entity = entities.Read(token.entity);
        return entity.owner_world.read.returned && entity.owner_world.value == world &&
               !TrackedEntities().IsDisposed(token) && !TrackedEntities().IsRetiring(token);
    }
    std::optional<Binding> Bind(EntityToken token, GameWorld* world) {
        if (!Owned(token, world))
            return {};
        const auto model = entities.Component(token.entity, model_type);
        if (!model.call.returned || !model.object || !Owned(token, world))
            return {};
        Keep(model.object, temporary);
        const auto seen = models.Read(static_cast<CharacterModelComponent*>(model.object));
        if (!seen.art.read.returned || !seen.animator.read.returned || !seen.changing.read.returned ||
            seen.changing.value)
            return {};
        Keep(seen.art.value, temporary);
        Keep(seen.animator.value, temporary);
        void* instance = nullptr;
        if (!native::Call(mock, &instance, token.entity).returned || !instance)
            return {};
        Keep(instance, temporary);
        void* actual = nullptr;
        if (!native::Call(mock_animator, &actual, instance).returned || actual != seen.animator.value ||
            !Owned(token, world))
            return {};
        return Binding{model.object, seen.art.value, seen.animator.value, instance};
    }
    bool Current(EntityToken token, GameWorld* world, const Binding& expected) {
        if (!Owned(token, world))
            return false;
        const auto component = entities.Component(token.entity, model_type);
        if (!component.call.returned || component.object != expected.model)
            return false;
        const auto current = models.Read(static_cast<CharacterModelComponent*>(component.object));
        if (!current.changing.read.returned || current.changing.value || !current.art.read.returned ||
            current.art.value != expected.art || !current.animator.read.returned ||
            current.animator.value != expected.animator)
            return false;
        void* actual = nullptr;
        if (!native::Call(mock, &actual, token.entity).returned || actual != expected.mock)
            return false;
        void* animator = nullptr;
        return native::Call(mock_animator, &animator, actual).returned && animator == expected.animator &&
               Owned(token, world);
    }
    void PrepareEffects() {
        if (effects_prepared) {
            if (!effect_error.empty())
                throw std::runtime_error(effect_error);
            return;
        }
        effects_prepared = true;
        try {
            auto& m = *metadata;
            ResolveAnimationParameterEffects(m, *this);
        } catch (const std::exception& e) {
            effect_error = e.what();
            throw;
        }
    }
    bool CurrentEffect(void* object, EffectReference ref) {
        if (Read<void*>(object, request_offset) != ref.parameters)
            return false;
        const auto facts = relations->Read(ref);
        return facts.same_generation && facts.released.read.returned && !facts.released.value &&
               facts.instance.call.returned && facts.instance.object == object;
    }
    std::vector<ParameterEffect> Effects(EntityToken token, GameWorld* world,
                                         const std::function<ObjectResult()>& manager, const Source& source) {
        PrepareEffects();
        if (!manager || !source)
            throw std::runtime_error("SyncPropState ownership services unavailable");
        auto* receiver = Require(manager(), "effect manager unavailable");
        void* list = nullptr;
        if (!native::Call(find_effects, &list, receiver, token.entity, false).returned)
            throw std::runtime_error("associated effect lookup failed");
        if (!list)
            return {};
        Keep(list, temporary);
        const auto count = effect_list->Count(list);
        if (!count.read.returned || count.value < 0)
            throw std::runtime_error("associated effect count unreadable");
        std::vector<ParameterEffect> result;
        for (int i = 0; i < count.value; ++i) {
            const auto item = effect_list->Item(list, i);
            if (!item.call.returned)
                throw std::runtime_error("associated effect item unreadable");
            if (!item.object)
                continue;
            auto* object = item.object;
            Keep(object, temporary);
            ParameterEffect row;
            auto* request = Read<void*>(object, request_offset);
            if (!request) {
                result.push_back(std::move(row));
                continue;
            }
            Keep(request, temporary);
            const EffectReference ref{Read<std::uint32_t>(request, id_offset), 0, request};
            const auto facts = relations->Read(ref);
            Keep(facts.caster.object, temporary);
            Keep(facts.target.object, temporary);
            row.owned =
                CurrentEffect(object, ref) && SameOwner(source(ref, facts), token, world) && CurrentEffect(object, ref);
            if (row.owned)
                row.synchronized = Read<bool>(request, sync_offset);
            if (row.owned && row.synchronized) {
                void* host = nullptr;
                void* children = nullptr;
                if (!native::Call(behavior, &host, object).returned)
                    throw std::runtime_error("effect behavior unreadable");
                if (host) {
                    Keep(host, temporary);
                    if (!native::Call(animators, &children, host).returned)
                        throw std::runtime_error("effect Animators unreadable");
                    if (children) {
                        Keep(children, temporary);
                        const auto length = animator_list->Count(children);
                        if (!length.read.returned || length.value < 0)
                            throw std::runtime_error("effect Animator count unreadable");
                        for (int j = 0; j < length.value; ++j) {
                            const auto child = animator_list->Item(children, j);
                            if (!child.call.returned)
                                throw std::runtime_error("effect Animator item unreadable");
                            if (child.object) {
                                Keep(child.object, temporary);
                                row.animators.push_back(child.object);
                            }
                        }
                    }
                }
                row.current = [this, object, ref] { return CurrentEffect(object, ref); };
            }
            result.push_back(std::move(row));
        }
        return result;
    }
};
ActorAnimationParameters::ActorAnimationParameters(RuntimeMetadata& metadata, ReferenceMethods refs,
                                                   EntityAccess entities, ModelAccess models, EntityOwner owner)
    : state_(std::make_unique<State>(metadata, refs, std::move(entities), std::move(models), std::move(owner))) {
    auto& s = *state_;
    try {

        ResolveAnimationParameterBody(metadata, s, [&](ObjectResult type) { s.Keep(type.object, s.types); });
        try {
            s.PrepareEffects();
        } catch (const std::exception&) {
        } // independent optional group, error retained
    } catch (...) {
        Close();
        throw;
    }
}
ActorAnimationParameters::~ActorAnimationParameters() = default;
const std::string& ActorAnimationParameters::PropagationError() const {
    return state_->effect_error;
}
AnimationParameterResult ActorAnimationParameters::Apply(EntityToken token, GameWorld* world,
                                                         const AnimationParameter& input,
                                                         AnimationParameterPropagation propagation,
                                                         const std::function<ObjectResult()>& manager,
                                                         const Source& source) {
    auto& s = *state_;
    AnimationParameterResult out;
    if (s.closed || s.busy) {
        out.detail = "animation parameter service closed or reentered";
        return out;
    }
    const auto value = input;
    s.busy = true;
    struct Exit {
        bool& flag;
        ~Exit() {
            flag = false;
        }
    } exit{s.busy};
    try {
        s.Keep(token.entity, s.temporary);
        s.Keep(world, s.temporary);
        const auto binding = s.Bind(token, world);
        if (!binding) {
            out.state = AnimationParameterState::OwnerChanged;
            out.detail = "owned actor model unavailable";
        } else {
            auto* name =
                s.Require(s.metadata->NewStringUtf8(value.name.c_str()), "animation parameter name allocation failed");
            out = ApplyAnimationParameter({[&] { return s.Current(token, world, *binding); },
                                           [&](const AnimationParameter& parameter) {
                                               return WriteAnimationParameter(s.body, binding->mock, name, parameter);
                                           },
                                           [&] { return s.Effects(token, world, manager, source); },
                                           [&](void* animator, const AnimationParameter& parameter) {
                                               return WriteAnimationParameter(s.effect, animator, name, parameter);
                                           }},
                                          value, propagation);
            out.model_identity = reinterpret_cast<std::uintptr_t>(binding->model);
        }
    } catch (const std::exception& e) {
        out.state = AnimationParameterState::Failed;
        out.detail = e.what();
    }
    out.references_released = s.Release(s.temporary);
    return out;
}
bool ActorAnimationParameters::Close() {
    auto& s = *state_;
    if (s.busy)
        return false;
    s.closed = true;
    s.model_type = nullptr;
    const bool a = s.Release(s.temporary), b = s.Release(s.types);
    return a && b;
}
}
