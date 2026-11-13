#include <hksr/presentation/events/EventValueInputs.hpp>
#include "../../../adapters/presentation/EventValueBindings.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::api {
struct EventValueInputs::State {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    using Field = detail::EventValueFieldBinding;
    std::map<detail::EventValueField, Field> fields;
    void* owner = nullptr;
    void* original_values = nullptr;
    void* original_strings = nullptr;
    void* values = nullptr;
    void* strings = nullptr;
    bool attempted = false, values_written = false, strings_written = false, closed = false;
    std::string error, operation = "read event context";
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
        : methods(m), metadata(d), objects(o), roots(r) {}
    void* Keep(ObjectResult v, bool nullable = false) {
        if (!v.call.returned || (!nullable && !v.object))
            throw std::runtime_error("event value input unavailable");
        if (!v.object)
            return nullptr;
        if (retained.count(v.object))
            return v.object;
        const auto id = roots.Keep(v.object);
        held.push_back(id);
        if (!roots.Status(id).ready)
            throw std::runtime_error("event value input retention failed");
        retained.insert(v.object);
        return v.object;
    }
    template <class... A> void* Object(Method m, A... a) {
        ObjectResult r;
        r.call = native::Call(m, &r.object, a...);
        if (!r.call.returned)
            throw std::runtime_error("read call did not return; exception=" + std::to_string(r.call.exception));
        return Keep(r, true);
    }
    template <class... A> void Call(Method m, A... a) {
        const auto r = native::CallVoid(m, a...);
        if (!r.returned)
            throw std::runtime_error("call did not return; exception=" + std::to_string(r.exception));
    }
    Field& Resolve(detail::EventValueField role) {
        const auto found = fields.find(role);
        if (found != fields.end())
            return found->second;
        auto field = detail::ResolveEventValueField(methods, metadata, objects, role,
                                                    [this](ObjectResult value) { return Keep(value); });
        return fields.emplace(role, std::move(field)).first->second;
    }
    void* Get(Field& f, void* from) {
        return Object(f.get, f.object, from);
    }
    void Set(Field& f, void* to, void* value) {
        Call(f.set, f.object, to, value, 0, static_cast<void*>(nullptr), static_cast<void*>(nullptr));
    }
    bool Undo(Field& field, void* replacement, void* original, bool& written) {
        if (!written)
            return true;
        auto* current = Get(field, owner);
        if (current == replacement)
            Set(field, owner, original);
        else if (current != original)
            throw std::runtime_error("event value table replaced by another owner; no overwrite");
        if (Get(field, owner) != original)
            throw std::runtime_error("event value restoration not observed");
        written = false;
        return true;
    }
};
EventValueInputs::EventValueInputs(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
    : state_(new State(m, d, o, r)) {}
EventValueInputs::~EventValueInputs() {
    delete state_;
}
const std::string& EventValueInputs::Error() const {
    return state_->error;
}
bool EventValueInputs::Apply(void* component, const std::string& path) {
    auto& s = *state_;
    if (s.attempted || s.closed)
        return false;
    s.attempted = true;
    try {
        if (!component || path.empty())
            throw std::runtime_error("event value source/component absent");
        const auto binding = detail::ResolveEventValueMethods(s.methods);
        auto* context = s.Object(binding.context, component);
        if (!context)
            throw std::runtime_error("event context absent");
        s.owner = s.Object(binding.owner, context);
        if (!s.owner)
            throw std::runtime_error("owned entity custom data absent");
        auto& numeric = s.Resolve(detail::EventValueField::Numeric);
        auto& text = s.Resolve(detail::EventValueField::Strings);
        s.original_values = s.Get(numeric, s.owner);
        s.original_strings = s.Get(text, s.owner);
        s.operation = "construct detached private tables";
        auto* stage = s.Keep(detail::AllocateEventValueStage(s.objects));
        s.Call(binding.stage_ctor, stage);
        s.values = s.Get(numeric, stage);
        s.strings = s.Get(text, stage);
        if (!s.values || !s.strings || s.values == s.original_values || s.strings == s.original_strings)
            throw std::runtime_error("private event tables were not constructed independently");
        // Preserve existing entity data, then use the official initialization
        // order: global numeric defaults, character numeric defaults, strings.
        const auto merge = binding.merge_numeric;
        s.Call(merge, stage, s.original_values);
        if (s.original_strings) {
            s.operation = "copy original string dictionary";
            s.Call(detail::ResolveEventStringDictionaryCopy(s.metadata, s.strings), s.strings, s.original_strings);
        }
        s.operation = "copy global numeric defaults";
        auto* global = s.Get(s.Resolve(detail::EventValueField::GlobalConfiguration), nullptr);
        if (!global)
            throw std::runtime_error("global event defaults unavailable");
        s.Call(merge, stage, s.Get(s.Resolve(detail::EventValueField::GlobalNumericDefaults), global));
        s.operation = "load character defaults: " + path;
        auto* name = s.Keep(s.objects.NewStringUtf8(path.c_str()));
        auto* config = s.Object(binding.load_character, name);
        if (!config)
            throw std::runtime_error("character event defaults unavailable");
        s.operation = "copy character numeric defaults";
        s.Call(merge, stage, s.Get(s.Resolve(detail::EventValueField::CharacterNumericDefaults), config));
        s.operation = "copy character string defaults";
        s.Call(binding.merge_strings, stage,
               s.Get(s.Resolve(detail::EventValueField::CharacterStringDefaults), config));
        // Prepare both tables before publishing either. Mark before the call so
        // a reflection failure after assignment still has a restoration owner.
        s.operation = "publish numeric table";
        s.values_written = true;
        s.Set(numeric, s.owner, s.values);
        s.operation = "publish string table";
        s.strings_written = true;
        s.Set(text, s.owner, s.strings);
        s.operation = "observe both published tables";
        if (s.Get(numeric, s.owner) != s.values || s.Get(text, s.owner) != s.strings)
            throw std::runtime_error("event value adoption not observed");
        return true;
    } catch (const std::exception& e) {
        s.error = s.operation + ": " + e.what();
        const auto original = s.error;
        Restore();
        s.error = original + (s.error == original ? "" : "; restore: " + s.error);
        return false;
    }
}
bool EventValueInputs::Restore() {
    auto& s = *state_;
    bool ok = true;
    if (!s.values_written && !s.strings_written)
        return true;
    try {
        s.Undo(s.Resolve(detail::EventValueField::Strings), s.strings, s.original_strings, s.strings_written);
    } catch (const std::exception& e) {
        s.error = e.what();
        ok = false;
    }
    try {
        s.Undo(s.Resolve(detail::EventValueField::Numeric), s.values, s.original_values, s.values_written);
    } catch (const std::exception& e) {
        s.error = e.what();
        ok = false;
    }
    return ok;
}
bool EventValueInputs::Close(bool ended) {
    auto& s = *state_;
    if (s.closed)
        return true;
    if (!ended && (s.values_written || s.strings_written) && !Restore())
        return false;
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
