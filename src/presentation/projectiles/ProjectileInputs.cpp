#include <hksr/presentation/projectiles/ProjectileInputs.hpp>
#include "../../../adapters/presentation/ProjectileInputBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>

namespace hksr::api {
struct ProjectileInputs::State {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    ProjectileInputStatus status;
    bool attempted = false;
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
        : methods(m), metadata(d), objects(o), roots(r) {}
    void* Keep(void* value) {
        if (!value || retained.count(value))
            return value;
        held.reserve(held.size() + 1);
        held.push_back(roots.Keep(value));
        if (!roots.Status(held.back()).ready)
            throw std::runtime_error("projectile input retention failed");
        retained.insert(value);
        return value;
    }
    void* Object(ObjectResult r) {
        if (!r.call.returned || !r.object)
            throw std::runtime_error("projectile input allocation failed");
        return Keep(r.object);
    }
    template <class... A> void* Invoke(Method m, A... args) {
        ObjectResult r;
        r.call = native::Call(m, &r.object, args...);
        return Object(r);
    }
    template <class T> T Read(void* p, std::size_t offset) {
        T out{};
        if (!p || !native::CopyMemoryChecked(&out, static_cast<char*>(p) + offset, sizeof(out)))
            throw std::runtime_error("projectile input read failed");
        return out;
    }
    void Reference(void* copy, const DeclaredField& f, void* value) {
        auto* type = Object(objects.ManagedType(f.owner_namespace.c_str(), f.owner_name.c_str()));
        auto* name = Object(objects.NewStringUtf8(f.name.c_str()));
        auto* field = Invoke(methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                             {"System.String", "System.Reflection.BindingFlags"}),
                             type, name, 54);
        const auto actual = metadata.ReadObjectType(field);
        MetadataResolver resolver({actual});
        const auto set = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                          {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                           "System.Reflection.Binder", "System.Globalization.CultureInfo"});
        if (!native::CallVoid(set, field, copy, value, 0, static_cast<void*>(nullptr), static_cast<void*>(nullptr))
                 .returned)
            throw std::runtime_error("projectile private reference write failed: " + f.name);
    }
    template <class T> void Scalar(void* copy, const DeclaredField& f, T value) {
        if (!native::CopyMemoryChecked(static_cast<char*>(copy) + f.offset, &value, sizeof(value)))
            throw std::runtime_error(std::string("projectile private value write failed: ") + f.name);
    }
};
ProjectileInputs::ProjectileInputs(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
    : state_(std::make_unique<State>(m, d, o, r)) {}
ProjectileInputs::~ProjectileInputs() = default;
const ProjectileInputStatus& ProjectileInputs::Status() const {
    return state_->status;
}
bool ProjectileInputs::Prepare(void* source, const std::map<std::string, float>& values) {
    return Prepare(source, values, {});
}
bool ProjectileInputs::Prepare(void* source, const std::map<std::string, float>& values, ResourceTransform transform) {
    auto& s = *state_;
    auto& out = s.status;
    if (s.attempted || out.closed)
        return false;
    s.attempted = true;
    try {
        if (!source)
            throw std::invalid_argument("projectile source absent");
        s.Keep(source);
        const auto type = s.metadata.ReadObjectTypeName(source);
        const auto schema = detail::ResolveProjectileInputSchema(s.metadata, source);
        out.legacy = schema.legacy;
        out.family = schema.family;

        std::vector<DeclaredField> numeric;
        std::set<std::string> used;
        const auto required = detail::ProjectileRequiredNumericInputs(out.family);
        for (const auto& f : schema.numeric) {
            if (!used.insert(f.name).second)
                throw std::runtime_error("ambiguous projectile numeric field: " + f.name);
            auto* dynamic = s.Read<void*>(source, f.offset);
            s.Keep(dynamic);
            const auto replacement = values.find(f.name);
            if (replacement != values.end()) {
                if (!std::isfinite(replacement->second))
                    throw std::invalid_argument("non-finite projectile value: " + f.name);
                numeric.push_back(f);
            } else if (dynamic && required.count(f.name)) {
                const auto expr = detail::ResolveProjectileExpressionField(s.metadata, dynamic);
                if (s.Read<void*>(dynamic, expr.offset))
                    out.unresolved_values.push_back(f.name);
            }
        }
        for (const auto& v : values)
            if (!used.count(v.first))
                throw std::invalid_argument("unknown projectile value: " + v.first);
        if (!out.unresolved_values.empty())
            throw std::invalid_argument("projectile expressions require evaluated inputs");
        const auto clone = detail::ResolveProjectileInputClone(s.methods);
        auto* copy = s.Invoke(clone, source);
        if (copy == source)
            throw std::runtime_error("projectile clone aliases its source");
        const auto copied = s.metadata.ReadObjectTypeName(copy);
        if (copied.name_space != type.name_space || copied.name != type.name)
            throw std::runtime_error("projectile clone type differs");
        if (!numeric.empty()) {
            const auto convert = detail::ResolveProjectileDynamicFloat(s.methods);
            for (const auto& f : numeric)
                s.Reference(copy, f, s.Invoke(convert, values.at(f.name)));
        }
        if (transform) {
            const ComponentConfig text(detail::ResolveProjectileResourceText(s.methods));
            for (const auto& field : {schema.fly_effect, schema.hit_effect}) {
                auto* original = s.Read<void*>(copy, field.offset);
                if (!original)
                    continue;
                s.Keep(original);
                const auto value = text.ReadText(original);
                if (!value.read.returned)
                    throw std::runtime_error("projectile resource name unreadable: " + value.error);
                const auto mapped = transform(value.utf8);
                if (!mapped)
                    s.Reference(copy, field, nullptr);
                else if (*mapped != value.utf8) {
                    if (mapped->find('\0') != std::string::npos)
                        throw std::invalid_argument("mapped projectile resource contains NUL");
                    s.Reference(copy, field, s.Object(s.objects.NewStringUtf8(mapped->c_str())));
                }
            }
        }
        const auto read_bool = [&](const DeclaredField& field) { return s.Read<bool>(copy, field.offset); };
        const auto& hit = schema.hit_effect;
        auto* fly = s.Read<void*>(copy, schema.fly_effect.offset);
        const auto nonempty = [&](void* text) {
            if (!text)
                return false;
            s.Keep(text);
            int length = 0;
            const auto r = native::Call(detail::ResolveProjectileEffectNameLength(s.methods), &length, text);
            if (!r.returned || length < 0)
                throw std::runtime_error("projectile effect name unreadable");
            return length != 0;
        };
        out.expects_effect = nonempty(fly);
        out.hit_effect = s.Keep(s.Read<void*>(copy, hit.offset));
        out.expects_hit = nonempty(out.hit_effect);
        out.align_hit = read_bool(schema.align_hit);
        out.attach_hit = read_bool(schema.attach_hit);
        out.start_from_last_hit = read_bool(schema.start_from_last);
        out.use_last_velocity = read_bool(schema.use_last_velocity);
        // References use reflection's GC barrier; only private blittable bools
        // use direct stores. No mutation of shared parameter/curve children.
        s.Reference(copy, hit, nullptr);
        s.Reference(copy, schema.fly_unique_name, nullptr);
        s.Scalar(copy, schema.trigger_hit_callback, false);
        s.Scalar(copy, schema.enable_raycast, false);
        s.Scalar(copy, schema.correct_formation, false);
        out.parameters = copy;
        out.prepared = true;
        return true;
    } catch (const std::exception& e) {
        out.error = e.what();
        Close();
        return false;
    }
}
bool ProjectileInputs::Close() {
    auto& s = *state_;
    auto& out = s.status;
    out.prepared = false;
    out.parameters = nullptr;
    out.hit_effect = nullptr;
    for (auto it = s.held.begin(); it != s.held.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.held.erase(it);
        } else
            ++it;
    }
    out.closed = s.held.empty();
    return out.closed;
}
}
