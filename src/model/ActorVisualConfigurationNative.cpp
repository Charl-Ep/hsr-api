#include <hksr/model/ActorVisualConfiguration.hpp>
#include "../../adapters/model/VisualConfigurationBindings.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace hksr::api {
namespace {
void Require(bool ok, const char* text) {
    if (!ok)
        throw std::runtime_error(text);
}
template <class T> T Read(void* p, std::size_t offset) {
    T v{};
    Require(p && native::CopyMemoryChecked(&v, static_cast<char*>(p) + offset, sizeof(v)),
            "visual configuration field read failed");
    return v;
}
template <class T> void Write(void* p, std::size_t offset, T v) {
    Require(p && native::CopyMemoryChecked(static_cast<char*>(p) + offset, &v, sizeof(v)),
            "visual configuration value write failed");
}
}
struct ActorVisualConfigurationFactory::State {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedArrays arrays;
    Fields fields;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    bool closed = false;
    struct Field {
        void* object = nullptr;
        Method get, set;
    };
    Field overlay, radius, somato, adaptations, graph_emotion, event_files, path, alias, attach;
    std::size_t data_owner = 0, emotion_owner = 0, single_value = 0, somato_value = 0, position = 0, rotation = 0,
                scale = 0;
    std::size_t consumer_paths = 0, consumer_layers = 0, consumer_ready = 0;
    void* adaptation_type = nullptr;
    void* string_type = nullptr;
    Method adaptation_changed, get_adaptations, get_events, bind_events, emotion_enable, emotion_blendshape;
    std::string data_error, radius_error, somato_error, adaptations_error, adaptation_entries_error;
    std::string adaptation_refresh_error, graph_emotion_error, data_events_error, events_read_error;
    std::string consumer_error, emotion_error, strings_error;
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods refs, ManagedArrays a, Fields f)
        : methods(m), metadata(d), objects(o), arrays(std::move(a)), fields(std::move(f)), roots(refs) {}
    ManagedRootId Keep(void* p) {
        if (!p)
            return 0;
        const auto id = roots.Keep(p);
        held.push_back(id);
        Require(roots.Status(id).ready, "visual configuration GC retention failed");
        return id;
    }
    void Release(const std::vector<ManagedRootId>& ids) {
        for (auto id : ids) {
            const auto it = std::find(held.begin(), held.end(), id);
            if (it != held.end() && id && roots.Release(id).complete) {
                roots.Forget(id);
                held.erase(it);
            }
        }
    }
    bool Close() {
        closed = true;
        const auto ids = held;
        Release(ids);
        return held.empty();
    }
    struct Work {
        State& s;
        std::vector<ManagedRootId> ids;
        explicit Work(State& v) : s(v) {}
        ~Work() {
            s.Release(ids);
        }
        void* Pin(void* p) {
            if (p)
                ids.push_back(s.Keep(p));
            return p;
        }
        void* Object(ObjectResult r) {
            Require(r.call.returned, "visual configuration managed allocation failed");
            return Pin(r.object);
        }
        void* String(const std::string& v) {
            return Object(s.objects.NewStringUtf8(v.c_str()));
        }
    };
    Field Reflection(const DeclaredField& f) {
        const auto type = objects.ManagedType(f.owner_namespace.c_str(), f.owner_name.c_str());
        Require(type.call.returned && type.object, "visual configuration field owner unavailable");
        Keep(type.object);
        const auto name = objects.NewStringUtf8(f.name.c_str());
        Require(name.call.returned && name.object, "visual configuration field name unavailable");
        Keep(name.object);
        const auto get = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                         {"System.String", "System.Reflection.BindingFlags"});
        void* object = nullptr;
        Require(native::Call(get, &object, type.object, name.object, 54).returned && object,
                "visual configuration reflection identity unavailable");
        Keep(object);
        const auto actual = metadata.ReadObjectType(object);
        MetadataResolver resolver({actual});
        return {object,
                resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetValue", "System.Object",
                                 {"System.Object"}),
                resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                 {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                  "System.Reflection.Binder", "System.Globalization.CultureInfo"})};
    }
    void Set(const Field& f, void* target, void* value) {
        Require(native::CallVoid(f.set, f.object, target, value, 0, static_cast<void*>(nullptr),
                                 static_cast<void*>(nullptr))
                    .returned,
                "visual configuration field assignment failed; application may be partial");
    }
    void* Get(const Field& f, void* target, Work& work) {
        void* value = nullptr;
        Require(native::Call(f.get, &value, f.object, target).returned, "visual configuration overlay read failed");
        return work.Pin(value);
    }
    void* Box(float v, Work& w) {
        auto* b = w.Object(objects.AllocateObject("System", "Single"));
        Require(b != nullptr, "Single box absent");
        Write(b, single_value, v);
        return b;
    }
    void* Box(std::int32_t v, Work& w) {
        auto* b =
            w.Object(detail::AllocateVisualConfigurationObject(objects, detail::VisualConfigurationObject::Somato));
        Require(b != nullptr, "somato enum box absent");
        Write(b, somato_value, v);
        return b;
    }
    void Prepare() {
        const auto binding = detail::ResolveVisualConfiguration(methods, fields);
        data_error = binding.data_error;
        radius_error = binding.radius_error;
        somato_error = binding.somato_error;
        adaptations_error = binding.adaptations_error;
        adaptation_entries_error = binding.adaptation_entries_error;
        adaptation_refresh_error = binding.adaptation_refresh_error;
        graph_emotion_error = binding.graph_emotion_error;
        data_events_error = binding.data_events_error;
        events_read_error = binding.events_read_error;
        consumer_error = binding.consumer_error;
        emotion_error = binding.emotion_error;
        auto prepare = [&](std::string& error, const char* name, auto operation) {
            if (!error.empty())
                return;
            try {
                operation();
            } catch (const std::exception& failure) {
                error = std::string("visual configuration ") + name + ": " + failure.what();
            }
        };
        prepare(data_error, "data", [&] { overlay = Reflection(binding.overlay); });
        prepare(radius_error, "radius", [&] { radius = Reflection(binding.radius); });
        prepare(somato_error, "somato", [&] { somato = Reflection(binding.somato); });
        prepare(adaptations_error, "effect adaptations", [&] { adaptations = Reflection(binding.adaptations); });
        prepare(adaptation_entries_error, "effect adaptations", [&] {
            path = Reflection(binding.path);
            alias = Reflection(binding.alias);
            attach = Reflection(binding.attach);
            const auto type =
                detail::ResolveVisualConfigurationObjectType(objects, detail::VisualConfigurationObject::Adaptation);
            Require(type.call.returned && type.object, "effect adaptation Type absent");
            adaptation_type = type.object;
            Keep(adaptation_type);
        });
        prepare(graph_emotion_error, "graph emotion", [&] { graph_emotion = Reflection(binding.graph_emotion); });
        prepare(data_events_error, "data event files", [&] { event_files = Reflection(binding.event_files); });
        prepare(strings_error, "event strings", [&] {
            const auto strings = objects.ManagedType("System", "String");
            Require(strings.call.returned && strings.object, "String Type absent");
            string_type = strings.object;
            Keep(string_type);
        });
        data_owner = binding.data_owner;
        emotion_owner = binding.emotion_owner;
        single_value = binding.single_value;
        somato_value = binding.somato_value;
        position = binding.position;
        rotation = binding.rotation;
        scale = binding.scale;
        consumer_paths = binding.consumer_paths;
        consumer_layers = binding.consumer_layers;
        consumer_ready = binding.consumer_ready;
        adaptation_changed = binding.adaptation_changed;
        get_adaptations = binding.get_adaptations;
        get_events = binding.get_events;
        bind_events = binding.bind_events;
        emotion_enable = binding.emotion_enable;
        emotion_blendshape = binding.emotion_blendshape;
    }
    void* Adaptations(const std::vector<EffectAdaptionEntry>& input, Work& work) {
        if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            throw std::invalid_argument("too many effect adaptations");
        auto* array = work.Object(arrays.Create(adaptation_type, static_cast<int>(input.size())));
        Require(array != nullptr, "effect adaptation array absent");
        for (std::size_t i = 0; i < input.size(); ++i) {
            const auto& e = input[i];
            auto* object = work.Object(
                detail::AllocateVisualConfigurationObject(objects, detail::VisualConfigurationObject::Adaptation));
            Require(object != nullptr, "effect adaptation object absent");
            Set(path, object, work.String(e.path));
            Set(alias, object, e.alias ? work.String(*e.alias) : nullptr);
            Set(attach, object, work.String(e.attach_point));
            Write(object, position, e.position);
            Write(object, rotation, e.rotation);
            Write(object, scale, e.scale);
            Require(arrays.Set(array, object, static_cast<int>(i)).returned,
                    "effect adaptation element assignment failed");
        }
        return array;
    }
    void* Strings(const std::vector<std::string>& input, Work& work) {
        if (!strings_error.empty())
            throw std::runtime_error(strings_error);
        if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            throw std::invalid_argument("too many event files");
        auto* array = work.Object(arrays.Create(string_type, static_cast<int>(input.size())));
        Require(array != nullptr, "event paths array absent");
        for (std::size_t i = 0; i < input.size(); ++i)
            Require(arrays.Set(array, work.String(input[i]), static_cast<int>(i)).returned,
                    "event path assignment failed");
        return array;
    }
    struct Bound {
        std::shared_ptr<State> s;
        ActorVisualConfigurationBinding binding;
        std::vector<ManagedRootId> pins;
        bool closed = false, closing = false;
        bool baseline_captured = false;
        bool data_type_checked = false, emotion_type_checked = false, consumer_type_checked = false;
        void* baseline_paths = nullptr;
        void* baseline_layers = nullptr;
        Bound(std::shared_ptr<State> f, ActorVisualConfigurationBinding b) : s(std::move(f)), binding(std::move(b)) {}
        bool Current() const {
            return !closed && !closing && !s->closed && binding.current && binding.current();
        }
        bool Current(VisualConfigurationComponent use) const {
            return Current() && (!binding.current_component || binding.current_component(use));
        }
        void Check() const {
            Require(Current(), "visual configuration binding ended");
        }
        static void Prepared(const std::string& error) {
            if (!error.empty())
                throw std::runtime_error(error);
        }
        void CheckType(void* object, detail::VisualConfigurationObject kind, bool& checked) {
            if (checked)
                return;
            Require(detail::MatchesVisualConfigurationObject(s->metadata, object, kind),
                    "visual configuration component type differs");
            checked = true;
        }
        void Check(VisualConfigurationComponent use) {
            Require(Current(use), "visual configuration component replaced");
            if (use == VisualConfigurationComponent::Data && binding.data) {
                Prepared(s->data_error);
                CheckType(binding.data, detail::VisualConfigurationObject::Data, data_type_checked);
                Require(Read<void*>(binding.data, s->data_owner) == binding.owner.token.entity,
                        "visual configuration data owner differs");
            }
            if (use == VisualConfigurationComponent::Emotion && binding.emotion) {
                Prepared(s->emotion_error);
                CheckType(binding.emotion, detail::VisualConfigurationObject::Emotion, emotion_type_checked);
                Require(Read<void*>(binding.emotion, s->emotion_owner) == binding.owner.token.entity,
                        "visual configuration emotion owner differs");
            }
            if (use == VisualConfigurationComponent::Events && binding.event_consumer)
                CheckType(binding.event_consumer, detail::VisualConfigurationObject::EventConsumer,
                          consumer_type_checked);
        }
        void CaptureEventBaseline() {
            if (baseline_captured)
                return;
            Check(VisualConfigurationComponent::Events);
            Prepared(s->consumer_error);
            baseline_paths = Read<void*>(binding.event_consumer, s->consumer_paths);
            baseline_layers = Read<void*>(binding.event_consumer, s->consumer_layers);
            if (baseline_paths)
                pins.push_back(s->Keep(baseline_paths));
            if (baseline_layers)
                pins.push_back(s->Keep(baseline_layers));
            baseline_captured = true;
        }
        void BindEvents(void* paths, void* layers = nullptr) {
            Check(VisualConfigurationComponent::Events);
            Prepared(s->consumer_error);
            Require(binding.event_consumer != nullptr, "animation event component has no consumer");
            Require(Read<void*>(binding.event_consumer, s->consumer_ready) != nullptr,
                    "animation event consumer is not ready");
            auto* prior_layers = Read<void*>(binding.event_consumer, s->consumer_layers);
            Require(native::CallVoid(s->bind_events, binding.event_consumer, paths, layers).returned,
                    "animation event rebind failed; application may be partial");
            Require(Read<void*>(binding.event_consumer, s->consumer_paths) == paths,
                    "animation event consumer did not adopt requested paths");
            Require(Read<void*>(binding.event_consumer, s->consumer_layers) == (layers ? layers : prior_layers),
                    "animation event layer input was not preserved");
        }
        bool RefreshEvents(const ActorVisualConfigurationInput& input, void* old_events) const {
            if (input.event_files)
                return true;
            if (!input.reset || !old_events)
                return false;
            const auto count = s->arrays.Length(old_events);
            Require(count.read.returned && count.value >= 0, "old event override length unreadable");
            return count.value != 0;
        }
        bool Current(const ActorVisualConfigurationInput& input) {
            if (!Current(VisualConfigurationComponent::Data))
                return false;
            if (!input.reset && !input.event_files)
                return true;
            if (!binding.data || input.event_files)
                return Current(VisualConfigurationComponent::Events);
            Check(VisualConfigurationComponent::Data);
            Prepared(s->data_events_error);
            Work work(*s);
            auto* copy = s->Get(s->overlay, binding.data, work);
            Require(copy != nullptr, "character override value absent");
            auto* old_events = s->Get(s->event_files, copy, work);
            const bool refresh = RefreshEvents(input, old_events);
            return Current(VisualConfigurationComponent::Data) &&
                   (!refresh || Current(VisualConfigurationComponent::Events));
        }
        ActorVisualConfigurationResult Apply(const ActorVisualConfigurationInput& input) {
            // Data presence selects the official overlay or direct-consumer
            // branch. An unread Data lookup must not become a null component.
            Check(VisualConfigurationComponent::Data);
            Work work(*s);
            if (!binding.data) {
                ActorVisualConfigurationResult result;
                if (input.reset || input.radius)
                    result.unsupported_fields.push_back("radius");
                if (input.reset || input.somato)
                    result.unsupported_fields.push_back("somato");
                if (input.reset || input.effect_adaptations)
                    result.unsupported_fields.push_back("effect_adaptations");
                if (input.reset || input.graph_emotion)
                    result.unsupported_fields.push_back("graph_emotion");
                if (input.reset || input.event_files) {
                    Check(VisualConfigurationComponent::Events);
                    if (!binding.event_consumer)
                        result.unsupported_fields.push_back("event_files");
                    else {
                        CaptureEventBaseline();
                        if (input.event_files)
                            BindEvents(s->Strings(*input.event_files, work));
                        else {
                            BindEvents(baseline_paths, baseline_layers);
                            Require(Read<void*>(binding.event_consumer, s->consumer_layers) == baseline_layers,
                                    "event reset could not restore original layer input");
                        }
                    }
                }
                return result;
            }
            // Qualify only the requested fields before committing the overlay.
            if (input.reset || input.radius)
                Prepared(s->radius_error);
            if (input.reset || input.somato)
                Prepared(s->somato_error);
            if (input.reset || input.effect_adaptations)
                Prepared(s->adaptations_error);
            if (input.effect_adaptations)
                Prepared(s->adaptation_entries_error);
            if (input.reset || input.graph_emotion)
                Prepared(s->graph_emotion_error);
            if (input.reset || input.event_files)
                Prepared(s->data_events_error);
            if (input.event_files)
                Prepared(s->strings_error);
            auto* copy = s->Get(s->overlay, binding.data, work);
            Require(copy != nullptr, "character override value absent");
            // The native task passes the old OVERRIDE array, not the effective
            // default list. Do not widen refresh to unrelated default paths.
            void* old_effects = nullptr;
            if (input.reset || input.effect_adaptations)
                old_effects = s->Get(s->adaptations, copy, work);
            void* old_events = nullptr;
            if (input.reset || input.event_files)
                old_events = s->Get(s->event_files, copy, work);
            const bool refresh_events = RefreshEvents(input, old_events);
            if (refresh_events) {
                Check(VisualConfigurationComponent::Events);
                if (binding.has_event_component || binding.event_consumer) {
                    Prepared(s->consumer_error);
                    if (!(input.reset && input.event_files))
                        Prepared(s->events_read_error);
                }
            }
            const bool refresh_adaptations = bool(input.effect_adaptations) || (input.reset && old_effects);
            if (refresh_adaptations)
                Prepared(s->adaptation_refresh_error);
            if (input.reset || input.radius)
                s->Set(s->radius, copy, s->Box(input.reset ? -1.0f : *input.radius, work));
            if (input.reset || input.somato)
                s->Set(s->somato, copy,
                       s->Box(input.reset ? detail::VisualConfigurationTypes::reset_somato : *input.somato, work));
            if (input.reset || input.effect_adaptations)
                s->Set(s->adaptations, copy, input.reset ? nullptr : s->Adaptations(*input.effect_adaptations, work));
            if (input.reset || input.graph_emotion)
                s->Set(s->graph_emotion, copy,
                       input.reset || input.graph_emotion->empty() ? nullptr : work.String(*input.graph_emotion));
            if (input.reset || input.event_files)
                s->Set(s->event_files, copy, input.reset ? nullptr : s->Strings(*input.event_files, work));
            Check(VisualConfigurationComponent::Data);
            s->Set(s->overlay, binding.data, copy); // GC barrier; no task shell/notifications
            // The native task updates all data before rebinding events, then
            // refreshes effects. Only the consumer call is retained here;
            // _OnAnimEventConfigListChanged also broadcasts Notify 0x4d3.
            if (refresh_events && (binding.has_event_component || binding.event_consumer)) {
                Require(binding.event_consumer != nullptr,
                        "animation event component has no consumer; configuration already changed");
                void* current = nullptr;
                if (input.reset && input.event_files)
                    current = s->Strings(*input.event_files, work);
                else {
                    Require(native::Call(s->get_events, &current, binding.data).returned,
                            "effective event paths unreadable after write");
                    work.Pin(current);
                }
                Check(VisualConfigurationComponent::Data);
                BindEvents(current);
            }
            if (refresh_adaptations) {
                void* current = nullptr;
                Require(native::Call(s->get_adaptations, &current, binding.data).returned,
                        "new effect adaptations unreadable after write");
                work.Pin(current);
                Check(VisualConfigurationComponent::Data);
                Require(native::CallVoid(s->adaptation_changed, binding.data, old_effects, current).returned,
                        "actor effect adaptation refresh failed; configuration already changed");
            }
            return {};
        }
        bool Emotion(bool enabled, float out, float in) {
            Check(VisualConfigurationComponent::Emotion);
            if (!binding.emotion)
                return false;
            Require(native::CallVoid(s->emotion_enable, binding.emotion,
                                     detail::VisualConfigurationTypes::emotion_source, enabled, enabled ? in : out)
                        .returned,
                    "emotion transition failed; application may be partial");
            Check(VisualConfigurationComponent::Emotion);
            Require(native::CallVoid(s->emotion_blendshape, binding.emotion, enabled).returned,
                    "emotion blendshape enable failed; transition already submitted");
            return true;
        }
        bool Close() {
            closing = true;
            s->Release(pins);
            for (auto id : pins)
                if (std::find(s->held.begin(), s->held.end(), id) != s->held.end())
                    return false;
            pins.clear();
            closed = true;
            return true;
        }
    };
};
ActorVisualConfigurationFactory::ActorVisualConfigurationFactory(MethodResolver& m, ObjectMetadata& d,
                                                                 ManagedObjects& o, ReferenceMethods r, ManagedArrays a,
                                                                 Fields f)
    : state_(std::make_shared<State>(m, d, o, r, std::move(a), std::move(f))) {
    try {
        state_->Prepare();
    } catch (...) {
        state_->Close();
        throw;
    }
}
ActorVisualConfigurationFactory::~ActorVisualConfigurationFactory() = default;
std::unique_ptr<ActorVisualConfiguration>
ActorVisualConfigurationFactory::Bind(ActorVisualConfigurationBinding binding) {
    Require(!state_->closed && binding.owner.token.entity && binding.owner.token.generation && binding.owner.world &&
                binding.model && bool(binding.current),
            "owned visual configuration binding incomplete");
    auto b = std::make_shared<State::Bound>(state_, std::move(binding));
    try {
        auto keep = [&](void* object) {
            if (!object)
                return;
            b->pins.push_back(state_->Keep(object));
        };
        keep(b->binding.data);
        keep(b->binding.emotion);
        keep(b->binding.event_consumer);
        b->Check();
    } catch (...) {
        b->Close();
        throw;
    }
    return std::make_unique<ActorVisualConfiguration>(ActorVisualConfigurationServices{
        [b] { return b->Current(); }, [b](const auto& input) { return b->Apply(input); },
        [b](bool e, float out, float in) { return b->Emotion(e, out, in); }, [b] { return b->Close(); },
        [b](VisualConfigurationComponent use) { return b->Current(use); },
        [b](const ActorVisualConfigurationInput& input) { return b->Current(input); }});
}
bool ActorVisualConfigurationFactory::Close() {
    return state_->Close();
}
}
