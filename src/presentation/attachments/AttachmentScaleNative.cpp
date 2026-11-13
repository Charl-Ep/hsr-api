#include "AttachmentScaleNative.hpp"
#include "../../../adapters/presentation/AttachmentScaleBindings.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
struct NativeScale : AttachmentScaleBindings {
    using Field = AttachmentScaleReflectedField;
    struct Point {
        void* attachment = nullptr;
        void* component = nullptr;
        void* persistent = nullptr;
        void* constraint = nullptr;
        std::map<AttachmentScaleField, void*> original;
        std::vector<ManagedRootId> roots;
        std::set<void*> retained;
        AttachmentScaleValue written{};
        AttachmentScaleValue baseline{};
        bool created = false, dirty = false, submitted = false, destroyed = false, restored = false;
        std::uint64_t baseline_revision = 0;
    };
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedRoots roots;
    std::function<bool()> current;
    void* model;
    void* scale_type = nullptr;
    void* vector_box = nullptr;
    std::map<AttachmentScaleField, Field> fields;
    std::map<std::string, Point> points;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    Point* capture = nullptr;
    struct Capture {
        Point*& slot;
        Point* old;
        Capture(Point*& slot, Point* value) : slot(slot), old(slot) {
            slot = value;
        }
        ~Capture() {
            slot = old;
        }
    };
    NativeScale(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r, void* owner,
                std::function<bool()> c)
        : AttachmentScaleBindings(ResolveAttachmentScale(m)), methods(m), metadata(d), objects(o), roots(r),
          current(std::move(c)), model(owner) {}
    void Check() {
        if (!current())
            throw std::runtime_error("attachment scale model ended or replaced");
    }
    void PrepareInputs() {
        if (scale_type)
            return;
        Check();
        Keep(model);
        Check();
        const auto type = AttachmentScaleType(objects);
        Check();
        scale_type = Keep(type);
        Check();
    }
    void* Keep(void* value) {
        auto& values = capture ? capture->retained : retained;
        auto& ids = capture ? capture->roots : held;
        if (!value || values.count(value))
            return value;
        ids.push_back(0);
        ids.back() = roots.Keep(value);
        if (!roots.Status(ids.back()).ready)
            throw std::runtime_error("attachment scale retention failed");
        values.insert(value);
        return value;
    }
    void* Keep(ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("attachment scale object unavailable");
        return Keep(value.object);
    }
    template <class... A> void* Object(Method method, A... args) {
        Check();
        void* value = nullptr;
        if (!native::Call(method, &value, args...).returned)
            throw std::runtime_error("attachment scale object call failed");
        auto* retained_value = Keep(value);
        Check();
        return retained_value;
    }
    template <class... A> void Call(Method method, A... args) {
        Check();
        if (!native::CallVoid(method, args...).returned)
            throw std::runtime_error("attachment scale call failed");
    }
    Field& Reflection(AttachmentScaleField key) {
        Capture global(capture, nullptr);
        auto it = fields.find(key);
        if (it != fields.end())
            return it->second;
        return fields
            .emplace(key, ResolveAttachmentScaleField(methods, metadata, objects, key,
                                                      [this](ObjectResult value) {
                                                          Check();
                                                          return Keep(value);
                                                      }))
            .first->second;
    }
    void* Get(AttachmentScaleField key, void* object) {
        auto& field = Reflection(key);
        return Object(field.get, field.object, object);
    }
    void Set(AttachmentScaleField key, void* object, void* value) {
        auto& field = Reflection(key);
        Call(field.set, field.object, object, value, 0, static_cast<void*>(nullptr), static_cast<void*>(nullptr));
    }
    void* Vector(AttachmentScaleValue value) {
        Capture global(capture, nullptr);
        if (!vector_box)
            vector_box = Keep(AttachmentScaleVector(objects));
        WriteAttachmentScaleVector(metadata, vector_box, value);
        return vector_box;
    }
    bool Alive(void* component) {
        bool result = false;
        Check();
        if (!native::Call(alive, &result, component).returned)
            throw std::runtime_error("attachment scale Unity lifetime unread");
        return result;
    }
    bool Ours(Point& p) {
        return AttachmentScaleMatches(metadata, p.component, p.attachment, p.constraint, p.written);
    }
    void Snapshot(Point& p) {
        for (const auto field :
             {AttachmentScaleField::LastTarget, AttachmentScaleField::Target, AttachmentScaleField::Origin,
              AttachmentScaleField::Point, AttachmentScaleField::Constraint, AttachmentScaleField::Destroyed})
            p.original[field] = Get(field, p.component);
        p.constraint = AttachmentScaleConstraint(metadata, p.component);
    }
    void Apply(const std::string& name, AttachmentScaleValue value, bool withdrawing, std::uint64_t revision) {
        Check();
        PrepareInputs();
        auto old = points.find(name);
        if (old != points.end() && withdrawing && revision < old->second.baseline_revision) {
            Restore(name);
            return; // older scopes belong to the superseded native baseline
        }
        if (old != points.end() &&
            (!old->second.component || old->second.destroyed || old->second.restored || !Alive(old->second.component) ||
             AttachmentScaleDestroyed(metadata, old->second.component))) {
            if (withdrawing)
                return;    // withdrawal must not resurrect an official-ended driver
            Restore(name); // a new explicit input may capture the replacement driver
        }
        auto [it, inserted] = points.try_emplace(name);
        auto& p = it->second;
        Capture owned(capture, &p);
        if (inserted) {
            p.baseline_revision = revision;
            p.attachment = Object(find, model, Keep(objects.NewStringUtf8(name.c_str())), false);
            if (!p.attachment)
                throw std::runtime_error("attachment scale point absent: " + name);
            auto* transform = AttachmentScaleTransform(metadata, p.attachment);
            if (!transform || !Alive(transform))
                throw std::runtime_error("attachment scale point Transform absent");
            Keep(transform);
            p.persistent = Get(AttachmentScaleField::Persistent, p.attachment);
            if (!native::Call(local_scale, &p.baseline, p.attachment).returned)
                throw std::runtime_error("attachment original scale unread");
            p.component = Object(get_component, transform, scale_type);
            if (p.component && (!Alive(p.component) || AttachmentScaleDestroyed(metadata, p.component)))
                p.component = nullptr;
            if (p.component) {
                Snapshot(p);
            } else {
                p.created = true;
                p.dirty = true;
                auto* go = Object(game_object, transform);
                p.component = Object(add_component, go, scale_type);
                if (!p.component)
                    throw std::runtime_error("attachment scale component not created");
                Set(AttachmentScaleField::Origin, p.component, Vector(p.baseline));
            }
        }
        if (!p.component || p.destroyed || p.restored || !Alive(p.component))
            throw std::runtime_error("attachment scale component ended");
        // This adapter owns only its explicit override, not subsequent official writes.
        if (p.submitted && !p.dirty && !Ours(p)) {
            if (withdrawing)
                return;
            if (AttachmentScaleDestroyed(metadata, p.component))
                throw std::runtime_error("official attachment scale was destroyed");
            Snapshot(p);
            p.created = false; // explicit new input captures the newer official baseline
            p.baseline_revision = revision;
            p.persistent = Get(AttachmentScaleField::Persistent, p.attachment);
            if (!native::Call(local_scale, &p.baseline, p.attachment).returned)
                throw std::runtime_error("new official attachment scale unread");
        }
        p.dirty = true;
        p.written = value;
        Set(AttachmentScaleField::Point, p.component, p.attachment);
        Set(AttachmentScaleField::Target, p.component, Vector(value));
        p.submitted = true;
        p.dirty = false;
    }
    void Restore(const std::string& name) {
        auto it = points.find(name);
        if (it == points.end())
            return;
        auto& p = it->second;
        if (!p.restored && p.component && !p.destroyed && (p.dirty || p.submitted) && Alive(p.component) &&
            (p.dirty || Ours(p))) {
            if (p.created) {
                p.dirty = true;
                Set(AttachmentScaleField::Origin, p.component, Vector(p.baseline));
                Call(self_destroy, p.component);
                Set(AttachmentScaleField::Persistent, p.attachment, p.persistent);
                Call(destroy, p.component, 0.0f);
                p.destroyed = true;
            } else {
                p.dirty = true;
                for (const auto& [field, value] : p.original)
                    Set(field, p.component, value);
                Call(set_local_scale, p.attachment, p.baseline);
                Set(AttachmentScaleField::Persistent, p.attachment, p.persistent);
                p.dirty = false;
            }
        }
        p.restored = true;
        if (!Release(p.roots))
            throw std::runtime_error("attachment scale point references pending");
        points.erase(it);
    }
    bool Release(std::vector<ManagedRootId>& ids) {
        for (auto it = ids.begin(); it != ids.end();) {
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = ids.erase(it);
            } else
                ++it;
        }
        return ids.empty();
    }
    bool Close() {
        if (current())
            while (!points.empty())
                Restore(points.begin()->first);
        else {
            for (auto it = points.begin(); it != points.end();) {
                if (Release(it->second.roots))
                    it = points.erase(it);
                else
                    ++it;
            }
        }
        return Release(held) && points.empty();
    }
};
} // namespace
std::unique_ptr<AttachmentScale> NativeAttachmentScale(MethodResolver& methods, ObjectMetadata& metadata,
                                                       ManagedObjects& objects, ReferenceMethods refs, void* model,
                                                       std::function<bool()> current) {
    auto native = std::make_shared<NativeScale>(methods, metadata, objects, refs, model, std::move(current));
    return std::make_unique<AttachmentScale>(AttachmentScaleServices{
        [native] { return native->current(); },
        [native](const auto& name, auto value, bool withdrawing, std::uint64_t revision) {
            native->Apply(name, value, withdrawing, revision);
        },
        [native](const auto& name) { native->Restore(name); }, [native] { return native->Close(); }});
}
} // namespace hksr::api::detail
