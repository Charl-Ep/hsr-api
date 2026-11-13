#include "ModelScaleNative.hpp"
#include "../../adapters/model/ModelScaleBindings.hpp"
#include "../../adapters/model/ModelPresentationCalls.hpp"
#include <hksr/model/ActorModelPresentation.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <map>

namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T result{};
    if (!object || !native::CopyMemoryChecked(&result, static_cast<char*>(object) + offset, sizeof(result)))
        throw std::runtime_error("character scale snapshot unreadable");
    return result;
}
template <class T> void Write(void* object, std::size_t offset, T value) {
    if (!native::CopyMemoryChecked(static_cast<char*>(object) + offset, &value, sizeof(value)))
        throw std::runtime_error("character scale private scalar input write failed");
}
}
struct ModelScaleNative::State : ModelScaleBindings {
    std::shared_ptr<RuntimeMetadata> metadata;
    ReferenceMethods references;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    bool closed = false;
    State(RuntimeMetadata& m, ReferenceMethods refs)
        : metadata(RetainRuntimeMetadata(m)), references(refs), roots(refs) {}
    void* Keep(ObjectResult result) {
        if (!result.call.returned || !result.object)
            throw std::runtime_error("character scale preparation object unavailable");
        held.push_back(roots.Keep(result.object));
        if (!roots.Status(held.back()).ready)
            throw std::runtime_error("character scale preparation retention failed");
        return result.object;
    }
    bool Release() {
        for (auto it = held.begin(); it != held.end();)
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = held.erase(it);
            } else
                ++it;
        return held.empty();
    }
    struct Bound {
        std::shared_ptr<State> methods;
        EntityOrigin origin;
        void* model;
        void* art;
        std::function<bool()> current;
        std::function<void()> permit;
        std::function<bool()> release_owner;
        ManagedRoots roots;
        std::map<void*, ManagedRootId> kept;
        void* config = nullptr;
        void* helper = nullptr;
        void* dictionary = nullptr;
        std::vector<ModelScaleMultiplier> configured;
        bool closed = false, helper_ready = false;
        Bound(std::shared_ptr<State> m, EntityOrigin o, void* model_, void* art_, std::function<bool()> c,
              std::function<void()> p, std::function<bool()> release)
            : methods(std::move(m)), origin(o), model(model_), art(art_), current(std::move(c)), permit(std::move(p)),
              release_owner(std::move(release)), roots(methods->references) {}
        void Check() {
            if (closed || methods->closed || !current())
                throw std::runtime_error("character scale actor/model/Art ended");
            permit();
        }
        void* Keep(ObjectResult result) {
            if (!result.call.returned || !result.object)
                throw std::runtime_error("character scale private input unavailable");
            return Keep(result.object);
        }
        void* Keep(void* object) {
            if (!object || kept.count(object))
                return object;
            const auto id = roots.Keep(object);
            kept.emplace(object, id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("character scale private input retention failed");
            return object;
        }
        void Ensure() {
            Check();
            if (helper_ready)
                return;
            auto& m = *methods;
            config = Keep(m.metadata->AllocateObject(m.config_type));
            helper = Keep(m.metadata->AllocateObject(m.helper_type));
            // Reviewed ctor only allocates CharacterModelScaleEventParam and
            // retains these inputs. No TaskContext method or task begin runs.
            if (!ConstructModelScaleHelper(m.helper_ctor, helper, config).returned)
                throw std::runtime_error("character scale private helper construction failed");
            helper_ready = true;
        }
        void Configure(const ModelScaleInput& input) {
            Ensure();
            auto& m = *methods;
            bool same = configured.size() == input.multiplier.size();
            for (std::size_t i = 0; same && i < configured.size(); ++i)
                same = configured[i].height == input.multiplier[i].height &&
                       configured[i].factor == input.multiplier[i].factor;
            if (!same || (!input.multiplier.empty() && !dictionary)) {
                void* next = nullptr;
                if (!input.multiplier.empty()) {
                    next = Keep(m.metadata->AllocateObject(m.multiplier_type));
                    if (!native::CallVoid(m.dict_ctor, next).returned)
                        throw std::runtime_error("character scale multiplier construction failed");
                    for (const auto& row : input.multiplier)
                        if (!native::CallVoid(m.dict_add, next, row.height, row.factor).returned)
                            throw std::runtime_error("character scale multiplier insertion failed");
                }
                if (!native::CallVoid(m.multiplier_set, m.multiplier_field, config, next, 0,
                                      static_cast<void*>(nullptr), static_cast<void*>(nullptr))
                         .returned)
                    throw std::runtime_error("character scale multiplier assignment failed");
                // Older private dictionaries have no engine-resource lifetime.
                if (dictionary) {
                    const auto old = kept.find(dictionary);
                    if (old != kept.end() && roots.Release(old->second).complete) {
                        roots.Forget(old->second);
                        kept.erase(old);
                    }
                }
                dictionary = next;
                configured = input.multiplier;
            }
            Write(config, m.config_outline, input.outline_scale);
            Write(config, m.config_equal, input.equal_ratio_by_min);
            Check();
        }
        ModelScaleSnapshot ReadSnapshot() {
            Check();
            auto& m = *methods;
            ModelScaleSnapshot result;
            auto* factor_values = Read<void*>(model, m.factors);
            if (factor_values) {
                Keep(factor_values);
                bool present = false;
                if (!native::Call(m.contains, &present, factor_values, m.factor_key).returned)
                    throw std::runtime_error("character scale factor lookup failed");
                if (present && !native::Call(m.item, &result.factor, factor_values, m.factor_key).returned)
                    throw std::runtime_error("character scale factor unreadable");
            }
            void* shader = nullptr;
            if (!native::Call(m.shader_get, &shader, origin.token.entity).returned)
                throw std::runtime_error("character outline component unreadable");
            if (shader) {
                Keep(shader);
                bool live = false;
                if (!native::Call(m.alive, &live, shader).returned)
                    throw std::runtime_error("character outline lifetime unreadable");
                if (live) {
                    result.shader_identity = reinterpret_cast<std::uintptr_t>(shader);
                    result.outline_scale = Read<float>(shader, m.outline);
                }
            }
            Check();
            return result;
        }
        float Multiplier(const ModelScaleInput& input) {
            Configure(input);
            float result = 1;
            if (!ReadModelScaleMultiplier(methods->multiplier, &result, helper, origin.token.entity).returned)
                throw std::runtime_error("official character scale multiplier failed");
            Check();
            return result;
        }
        void Apply(const ModelScaleInput& input, float ratio) {
            Configure(input);
            if (!ApplyModelScaleHelper(methods->apply, helper, origin.token.entity, input.base_scale, ratio).returned)
                throw std::runtime_error("official character scale application failed; may be partial");
        }
        void Restore(ModelScaleSnapshot snapshot) {
            ModelScaleInput input;
            input.base_scale = snapshot.factor;
            input.outline_scale = snapshot.outline_scale;
            Apply(input, 1);
        }
        bool Close() {
            closed = true;
            bool complete = true;
            for (auto it = kept.begin(); it != kept.end();)
                if (roots.Release(it->second).complete) {
                    roots.Forget(it->second);
                    it = kept.erase(it);
                } else {
                    complete = false;
                    ++it;
                }
            return release_owner() && complete;
        }
    };
};
ModelScaleNative::ModelScaleNative(RuntimeMetadata& metadata, ReferenceMethods refs)
    : state_(std::make_shared<State>(metadata, refs)) {
    auto& s = *state_;
    try {
        static_cast<ModelScaleBindings&>(s) =
            ResolveModelScaleBindings(metadata, [&](ObjectResult object) { return s.Keep(object); });
    } catch (...) {
        Close();
        throw;
    }
}
ModelScaleNative::~ModelScaleNative() = default;
ModelScaleTarget ModelScaleNative::Bind(EntityOrigin origin, void* model, void* art, std::function<bool()> current,
                                        std::function<void()> permit, std::function<bool()> release) {
    auto s = std::make_shared<State::Bound>(state_, origin, model, art, std::move(current), std::move(permit),
                                            std::move(release));
    return {origin,
            reinterpret_cast<std::uintptr_t>(model),
            reinterpret_cast<std::uintptr_t>(art),
            [s] { return !s->closed && !s->methods->closed && s->current(); },
            [s] { return s->ReadSnapshot(); },
            [s](const auto& input) { s->Configure(input); },
            [s](const auto& input) { return s->Multiplier(input); },
            [s](const auto& input, float ratio) { s->Apply(input, ratio); },
            [s](auto baseline) { s->Restore(baseline); },
            [s] { return s->Close(); }};
}
bool ModelScaleNative::Close() {
    state_->closed = true;
    return state_->Release();
}
}
