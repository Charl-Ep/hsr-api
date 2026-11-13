#include "ActorBgmSyncNative.hpp"
#include <hksr/model/ModelAccess.hpp>
#include "../../adapters/animation/BgmSyncBindings.hpp"
#include "ActorBgmSyncCalls.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
template <class T> void Scalar(void* object, std::size_t offset, T value) {
    if (!native::CopyMemoryChecked(static_cast<char*>(object) + offset, &value, sizeof(value)))
        throw std::runtime_error("BGM sync private scalar write failed");
}
RuntimeField Field(RuntimeMetadata& m, RuntimeType owner, const char* name, const char* type,
                   std::size_t* offset = nullptr) {
    RuntimeField result;
    for (const auto& row : m.ObserveFields(owner).fields)
        if (row.error.empty() && row.declaration.name == name) {
            if (result || row.declaration.type != type || !row.declaration.flags_observed ||
                (row.declaration.flags & 16))
                throw std::runtime_error(std::string("BGM sync field differs: ") + name);
            result = row.field;
            if (offset)
                *offset = row.declaration.offset;
        }
    if (!result)
        throw std::runtime_error(std::string("BGM sync field absent: ") + name);
    return result;
}
}
struct ActorBgmSyncFactory::State : BgmSyncBindings {
    struct Client;
    std::shared_ptr<RuntimeMetadata> metadata;
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    ManagedArrays arrays;
    EntityOwner owner;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    std::vector<std::weak_ptr<Client>> clients;
    std::vector<std::shared_ptr<Client>> pending;
    bool closed = false;
    State(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess e, ModelAccess models_, ManagedArrays a,
          EntityOwner o)
        : metadata(RetainRuntimeMetadata(m)), references(refs), entities(std::move(e)), models(std::move(models_)),
          arrays(std::move(a)), owner(std::move(o)), roots(refs) {}
    void* Keep(ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("BGM sync metadata input unavailable");
        const auto id = roots.Keep(value.object);
        held.push_back(id);
        if (!roots.Status(id).ready)
            throw std::runtime_error("BGM sync metadata retention failed");
        return value.object;
    }
    BgmSyncInputType Type(const char* ns, const char* name) {
        return {metadata->FindType(ns, name), Keep(metadata->ManagedType(ns, name))};
    }
    BgmSyncReferenceField Ref(const BgmSyncInputType& type, const char* name, const char* expected) {
        Field(*metadata, type.kind, name, expected);
        auto* text = Keep(metadata->NewStringUtf8(name));
        const auto get = metadata->Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                           {"System.String", "System.Reflection.BindingFlags"});
        ObjectResult result;
        result.call = native::Call(get, &result.object, type.system, text, 54);
        auto* object = Keep(result);
        const auto actual = metadata->ReadObjectType(object);
        MetadataResolver resolver({actual});
        return {object, resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                         {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                          "System.Reflection.Binder", "System.Globalization.CultureInfo"})};
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
    struct Binding {
        void* model = nullptr;
        void* art = nullptr;
        void* animator = nullptr;
        void* controller = nullptr;
        void* component = nullptr;
        bool operator==(const Binding& b) const {
            return model == b.model && art == b.art && animator == b.animator && controller == b.controller &&
                   component == b.component;
        }
    };
    struct Client {
        struct Request {
            void* config = nullptr;
            void* helper = nullptr;
            std::vector<ManagedRootId> roots;
            bool accepted = false, drained = false;
            std::string error;
        };
        std::shared_ptr<State> factory;
        EntityToken token;
        GameWorld* world;
        Binding binding;
        ManagedRoots roots;
        std::vector<ManagedRootId> held;
        std::map<std::uint64_t, Request> requests;
        std::uint64_t next = 0;
        bool closing = false, closed = false, starting = false;
        Client(std::shared_ptr<State> f, EntityToken t, GameWorld* w)
            : factory(std::move(f)), token(t), world(w), roots(factory->references) {}
        bool Owned() {
            auto& f = *factory;
            if (closed || !world || !token.entity || !token.generation || TrackedEntities().IsDisposed(token) ||
                TrackedEntities().IsRetiring(token) || f.owner.OwnedToken(token.entity).generation != token.generation)
                return false;
            const auto entity = f.entities.Read(token.entity);
            if (!entity.owner_world.read.returned)
                throw std::runtime_error("BGM sync actor world unreadable");
            return entity.owner_world.value == world;
        }
        std::optional<Binding> ReadBinding() {
            auto& f = *factory;
            if (!Owned())
                return {};
            const auto model = f.entities.Component(token.entity, f.model_type);
            if (!model.call.returned)
                throw std::runtime_error("BGM sync model lookup failed");
            if (!model.object)
                return {};
            const auto view = f.models.Read(static_cast<CharacterModelComponent*>(model.object));
            if (!view.changing.read.returned || !view.art.read.returned || !view.animator.read.returned)
                throw std::runtime_error("BGM sync model observation failed");
            if (view.changing.value || !view.art.value || !view.animator.value)
                return {};
            const auto component = f.entities.Component(token.entity, f.component_type);
            if (!component.call.returned)
                throw std::runtime_error("BGM sync actor component lookup failed");
            void* component_animator = nullptr;
            if (component.object && !native::Call(f.animator, &component_animator, component.object).returned)
                throw std::runtime_error("BGM sync Animator getter failed");
            // A component for another animation domain is not a loading clock.
            // This current model has no applicable BGM helper.
            auto* selected = component.object && component_animator == view.animator.value ? component.object : nullptr;
            void* controller_object = nullptr;
            if (!native::Call(f.controller, &controller_object, view.animator.value).returned)
                throw std::runtime_error("BGM sync controller unreadable");
            if (!controller_object)
                return {};
            return Binding{model.object, view.art.value, view.animator.value, controller_object, selected};
        }
        bool Current() {
            if (closing || factory->closed)
                return false;
            const auto now = ReadBinding();
            return now && *now == binding;
        }
        void* Keep(void* object, std::vector<ManagedRootId>& into) {
            if (!object)
                return nullptr;
            const auto id = roots.Keep(object);
            into.push_back(id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("BGM sync object retention failed");
            return object;
        }
        void* Object(ObjectResult value, std::vector<ManagedRootId>& into) {
            if (!value.call.returned || !value.object)
                throw std::runtime_error("BGM sync input allocation failed");
            return Keep(value.object, into);
        }
        void Set(const BgmSyncReferenceField& field, void* object, void* value) {
            if (!native::CallVoid(field.set, field.field, object, value, 0, static_cast<void*>(nullptr),
                                  static_cast<void*>(nullptr))
                     .returned)
                throw std::runtime_error("BGM sync private reference assignment failed");
        }
        void* Text(const std::string& value, Request& request) {
            return Object(factory->metadata->NewStringUtf8(value.c_str()), request.roots);
        }
        void* Phases(const std::vector<BgmSyncPhase>& values, Request& request) {
            auto& f = *factory;
            auto* array = Object(f.arrays.Create(f.phase_type.system, static_cast<int>(values.size())), request.roots);
            for (std::size_t i = 0; i < values.size(); ++i) {
                auto* phase = Object(f.metadata->AllocateObject(f.phase_type.kind), request.roots);
                Scalar(phase, f.time_offset, values[i].start_time);
                Scalar(phase, f.blend_offset, values[i].blend);
                Scalar(phase, f.normalized_offset, values[i].normalized_offset);
                if (!f.arrays.Set(array, phase, static_cast<int>(i)).returned)
                    throw std::runtime_error("BGM sync phase array assignment failed");
            }
            return array;
        }
        void Prepare(const BgmSyncConfig& value, Request& request) {
            auto& f = *factory;
            request.config = Object(f.metadata->AllocateObject(f.config_type.kind), request.roots);
            Set(f.state_name, request.config, Text(value.state, request));
            Scalar(request.config, f.bpm_offset, value.bpm);
            auto* chapter_array =
                Object(f.arrays.Create(f.chapter_type.system, static_cast<int>(value.chapters.size())), request.roots);
            for (std::size_t i = 0; i < value.chapters.size(); ++i) {
                const auto& value_chapter = value.chapters[i];
                auto* chapter = Object(f.metadata->AllocateObject(f.chapter_type.kind), request.roots);
                Set(f.cue, chapter, Text(value_chapter.cue, request));
                Set(f.phases, chapter, Phases(value_chapter.phases, request));
                Set(f.double_phases, chapter,
                    value_chapter.double_speed_phases ? Phases(*value_chapter.double_speed_phases, request) : nullptr);
                if (!f.arrays.Set(chapter_array, chapter, static_cast<int>(i)).returned)
                    throw std::runtime_error("BGM sync chapter array assignment failed");
            }
            Set(f.chapters, request.config, chapter_array);
        }
        bool Active(Request& request) {
            if (request.drained || !request.config || !binding.component || !Owned())
                return false;
            const auto slot = ReadBgmSyncSlot(binding.component, factory->helper_offset, factory->config_offset);
            if (!OwnsBgmSyncSlot(slot, request.helper, request.config))
                return false;
            if (!request.helper)
                request.helper = Keep(slot.helper, request.roots);
            return true;
        }
        bool Release(Request& request) {
            for (auto it = request.roots.begin(); it != request.roots.end();)
                if (roots.Release(*it).complete) {
                    roots.Forget(*it);
                    it = request.roots.erase(it);
                } else
                    ++it;
            request.drained = request.roots.empty();
            if (request.drained) {
                request.config = nullptr;
                request.helper = nullptr;
            }
            return request.drained;
        }
        BgmSyncStartResult Start(const BgmSyncConfig& value) {
            if (starting || !Current())
                return {BgmSyncStartState::Failed, {}, "BGM sync actor/model/controller changed or start reentered"};
            if (!binding.component)
                return {BgmSyncStartState::Unsupported, {}, "owned actor has no official BGM animation component"};
            starting = true;
            struct Exit {
                bool& value;
                ~Exit() {
                    value = false;
                }
            } exit{starting};
            const BgmSyncToken id{++next};
            auto& request = requests[id.value];
            try {
                Prepare(value, request);
                if (!Current())
                    throw std::runtime_error("BGM sync actor changed while preparing inputs");
                const auto call = StartBgmSync(factory->start, binding.component, request.config);
                request.accepted = call.returned;
                if (!call.returned)
                    throw std::runtime_error("official BGM sync start failed; may be partial");
                if (closing)
                    throw std::runtime_error("BGM sync closed during start; cleanup retained");
                Active(request);
                CollectOld(id);
                return {BgmSyncStartState::Accepted, id, {}};
            } catch (const std::exception& e) {
                request.error = e.what();
                const auto error = request.error;
                CollectOld(id);
                return {BgmSyncStartState::Failed, id, error};
            }
        }
        void CollectOld(BgmSyncToken current) {
            std::vector<BgmSyncToken> ids;
            for (const auto& row : requests)
                if (row.first != current.value)
                    ids.push_back({row.first});
            for (const auto id : ids)
                Observe(id);
        }
        BgmSyncObservation Observe(BgmSyncToken id) {
            const auto found = requests.find(id.value);
            if (found == requests.end())
                return {false, false, true, {}};
            auto& request = found->second;
            try {
                if (!Current()) {
                    const bool accepted = request.accepted;
                    const auto error = request.error;
                    if (Stop(id))
                        return {accepted, false, true, error};
                    return {accepted, false, false, request.error};
                }
                const bool active = Active(request);
                if (!active)
                    Release(request);
                const BgmSyncObservation result{request.accepted, active, request.drained, request.error};
                if (request.drained)
                    requests.erase(found);
                return result;
            } catch (const std::exception& e) {
                request.error = e.what();
                return {request.accepted, false, false, request.error};
            }
        }
        bool Stop(BgmSyncToken id) {
            const auto found = requests.find(id.value);
            if (found == requests.end())
                return true;
            auto& request = found->second;
            try {
                if (Active(request)) {
                    const auto result =
                        StopOwnedBgmSync(factory->stop, binding.component, request.helper, request.config,
                                         factory->helper_offset, factory->config_offset);
                    if (!result.call.returned)
                        throw std::runtime_error("official BGM sync stop failed; may be partial");
                    if (result.active_after)
                        throw std::runtime_error("official BGM sync stop not observed");
                }
                const auto done = Release(request);
                if (done)
                    requests.erase(found);
                return done;
            } catch (const std::exception& e) {
                request.error = e.what();
                return false;
            }
        }
        bool Close() {
            closing = true;
            if (starting)
                return false;
            bool done = true;
            std::vector<BgmSyncToken> ids;
            for (const auto& row : requests)
                ids.push_back({row.first});
            for (const auto id : ids)
                done = Stop(id) && done;
            if (!done)
                return false;
            for (auto it = held.begin(); it != held.end();)
                if (roots.Release(*it).complete) {
                    roots.Forget(*it);
                    it = held.erase(it);
                } else
                    ++it;
            closed = held.empty();
            return closed;
        }
    };
};
ActorBgmSyncFactory::ActorBgmSyncFactory(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess entities,
                                         ModelAccess models, ManagedArrays arrays, EntityOwner owner)
    : state_(std::make_shared<State>(m, refs, std::move(entities), std::move(models), std::move(arrays),
                                     std::move(owner))) {
    auto& s = *state_;
    try {

        static_cast<BgmSyncBindings&>(s) = ResolveBgmSyncBindings(
            m, [&](const char* ns, const char* name) { return s.Type(ns, name); },
            [&](const BgmSyncInputType& type, const char* name, const char* expected) {
                return s.Ref(type, name, expected);
            });
    } catch (...) {
        Close();
        throw;
    }
}
ActorBgmSyncFactory::~ActorBgmSyncFactory() = default;
std::unique_ptr<ActorBgmSync> ActorBgmSyncFactory::Create(EntityToken token, GameWorld* world) {
    auto f = state_;
    if (f->closed)
        throw std::logic_error("BGM sync factory closed");
    f->clients.erase(
        std::remove_if(f->clients.begin(), f->clients.end(), [](const auto& weak) { return weak.expired(); }),
        f->clients.end());
    auto client = std::make_shared<State::Client>(f, token, world);
    const auto binding = client->ReadBinding();
    if (!binding)
        return {};
    client->binding = *binding;
    try {
        for (auto* object : {static_cast<void*>(token.entity), static_cast<void*>(world), binding->model, binding->art,
                             binding->animator, binding->controller, binding->component})
            client->Keep(object, client->held);
    } catch (...) {
        if (!client->Close())
            f->pending.push_back(client);
        throw;
    }
    f->clients.push_back(client);
    return std::make_unique<ActorBgmSync>(ActorBgmSyncServices{
        [client](const auto& config) { return client->Start(config); },
        [client](auto id) { return client->Observe(id); }, [client](auto id) { return client->Stop(id); },
        [client] { return client->Current(); }, [client] { return client->Close(); }});
}
bool ActorBgmSyncFactory::Close() {
    auto& s = *state_;
    s.closed = true;
    bool done = true;
    for (const auto& weak : s.clients)
        if (auto client = weak.lock())
            done = client->Close() && done;
    for (auto it = s.pending.begin(); it != s.pending.end();)
        if ((*it)->Close())
            it = s.pending.erase(it);
        else {
            done = false;
            ++it;
        }
    return done && s.Release();
}
}
