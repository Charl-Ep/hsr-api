#include "../../src/presentation/attachments/AttachmentScaleNative.hpp"
#include <Windows.h>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <functional>
using namespace hksr::api;
namespace {
using V = AttachmentScaleValue;
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("native scale regression");
}
struct Point {
    V local{1, 1, 1};
    std::optional<V> persistent;
    int marker = 0;
    void* transform = &marker;
};
struct Component {
    V last{1, 1, 1}, target{1, 1, 1}, origin{1, 1, 1};
    Point* point = nullptr;
    void* parent = nullptr;
    bool destroyed = false;
    bool alive = true;
};
struct Box {
    V vector{};
    bool boolean = false;
};
struct Type {
    std::string name;
};
struct Field {
    std::string owner, name;
};
Point point;
Component original, added;
Component* selected = nullptr;
int info, model, go, next = 0, registered = 0, destroyed = 0, ticks = 0;
bool current = true, fields_fail = false;
bool type_fail = false, free_fault = false;
int free_calls = 0, type_reads = 0, local_reads = 0;
std::function<void()> on_keep, on_write, on_persistent_read;
std::map<int, void*> roots;
std::vector<std::unique_ptr<std::string>> strings;
std::map<std::string, std::unique_ptr<Type>> types;
std::map<std::string, std::unique_ptr<Field>> fields;
std::vector<std::unique_ptr<Box>> boxes;
std::set<void*> boxed;
template <class F> Method M(F f) {
    return {reinterpret_cast<void*>(f), &info};
}
Box* NewBox() {
    boxes.push_back(std::make_unique<Box>());
    boxed.insert(boxes.back().get());
    return boxes.back().get();
}
void* Vector(V value) {
    auto* box = NewBox();
    box->vector = value;
    return box;
}
int __fastcall Keep(void* value, int, int, void*) {
    const auto id = ++next;
    roots[id] = value;
    if (on_keep) {
        auto callback = std::move(on_keep);
        on_keep = {};
        callback();
    }
    return id;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    ++free_calls;
    if (free_fault)
        RaiseException(0xE0424242, 0, 0, nullptr);
    roots.erase(id);
}
void* __fastcall Find(void* self, std::string* name, bool fallback, void*) {
    Check(self == &model && *name == "Arrow" && !fallback);
    return &point;
}
V __fastcall Local(Point* self, void*) {
    ++local_reads;
    return self->persistent.value_or(self->local);
}
void __fastcall SetLocal(Point* self, V value, void*) {
    self->local = value;
    self->persistent = value;
}
void* __fastcall GameObject(void* transform, void*) {
    Check(transform == point.transform);
    return &go;
}
void* __fastcall GetComponent(void* transform, Type* type, void*) {
    Check(transform == point.transform && type->name == ".AttachPointSet");
    return selected;
}
void* __fastcall Add(void* self, Type* type, void*) {
    Check(self == &go && type->name == ".AttachPointSet");
    added = {};
    selected = &added;
    ++registered;
    return selected;
}
bool __fastcall Alive(void* object, void*) {
    if (object == point.transform)
        return true;
    return static_cast<Component*>(object)->alive;
}
void __fastcall Tick(Component* self, void*) {
    Check(self->alive && !self->destroyed && self->point);
    ++ticks;
    if (self->target[0] >= 0 && self->target[1] >= 0 && self->target[2] >= 0) {
        self->point->local = self->target;
        self->point->persistent = self->target;
    }
    self->last = self->point->persistent.value_or(self->point->local);
}
void __fastcall SelfDestroy(Component* self, void*) {
    if (self->destroyed)
        return;
    --registered;
    if (self->point) {
        self->point->local = self->origin;
        self->point->persistent = self->origin;
    }
    self->point = nullptr;
    self->destroyed = true;
}
void __fastcall Destroy(Component* self, float seconds, void*) {
    Check(seconds == 0);
    self->alive = false;
    selected = nullptr;
    ++destroyed;
}
void* __fastcall GetField(Type* type, std::string* name, int flags, void*) {
    Check(flags == 54);
    const auto key = type->name + "." + *name;
    auto& result = fields[key];
    if (!result)
        result = std::make_unique<Field>(Field{type->name, *name});
    return result.get();
}
void* __fastcall ReadField(Field* field, void* object, void*) {
    const auto& name = field->name;
    if (field->owner == "RPG.Client.AttachPoint") {
        Check(name == "PersistentLocalScale");
        if (on_persistent_read) {
            auto callback = std::move(on_persistent_read);
            on_persistent_read = {};
            callback();
        }
        const auto& value = static_cast<Point*>(object)->persistent;
        return value ? Vector(*value) : nullptr;
    }
    auto& c = *static_cast<Component*>(object);
    if (name == "AttachPoint")
        return c.point;
    if (name == "ParentConstraint")
        return c.parent;
    if (name == "IsDestroy") {
        auto* box = NewBox();
        box->boolean = c.destroyed;
        return box;
    }
    if (name == "OriginScale")
        return Vector(c.origin);
    if (name == "LastTargetScale")
        return Vector(c.last);
    Check(name == "TargetScale");
    return Vector(c.target);
}
void __fastcall WriteField(Field* field, void* object, void* value, int, void*, void*, void*) {
    if (on_write) {
        auto callback = std::move(on_write);
        on_write = {};
        callback();
    }
    const auto& name = field->name;
    if (field->owner == "RPG.Client.AttachPoint") {
        Check(name == "PersistentLocalScale");
        auto& p = *static_cast<Point*>(object);
        p.persistent = value ? std::optional<V>(static_cast<Box*>(value)->vector) : std::nullopt;
        return;
    }
    auto& c = *static_cast<Component*>(object);
    if (name == "AttachPoint")
        c.point = static_cast<Point*>(value);
    else if (name == "ParentConstraint")
        c.parent = value;
    else if (name == "IsDestroy")
        c.destroyed = static_cast<Box*>(value)->boolean;
    else if (name == "OriginScale")
        c.origin = static_cast<Box*>(value)->vector;
    else if (name == "LastTargetScale")
        c.last = static_cast<Box*>(value)->vector;
    else {
        Check(name == "TargetScale");
        c.target = static_cast<Box*>(value)->vector;
    }
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char* ns, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> parameters, MethodKind kind) override {
        MetadataResolver resolver(
            {{"RPG.GameCore",
              "CharacterModelComponent",
              {{"GetAttachPointInst", "RPG.Client.AttachPoint", {"System.String", "System.Boolean"}, false, M(Find)}}},
             {"RPG.Client",
              "AttachPoint",
              {{"get_LocalScale", "UnityEngine.Vector3", {}, false, M(Local)},
               {"set_LocalScale", "System.Void", {"UnityEngine.Vector3"}, false, M(SetLocal)}}},
             {"UnityEngine",
              "Component",
              {{"get_gameObject", "UnityEngine.GameObject", {}, false, M(GameObject)},
               {"GetComponent", "UnityEngine.Component", {"System.Type"}, false, M(GetComponent)}}},
             {"UnityEngine", "GameObject", {{"AddComponent", "UnityEngine.Component", {"System.Type"}, false, M(Add)}}},
             {"UnityEngine",
              "Object",
              {{"op_Implicit", "System.Boolean", {"UnityEngine.Object"}, true, M(Alive)},
               {"Destroy", "System.Void", {"UnityEngine.Object", "System.Single"}, true, M(Destroy)}}},
             {"",
              "AttachPointSet",
              {{"SelfDestroy", "System.Void", {}, false, M(SelfDestroy)},
               {"LateTick", "System.Void", {}, false, M(Tick)}}},
             {"System",
              "RuntimeType",
              {{"GetField",
                "System.Reflection.FieldInfo",
                {"System.String", "System.Reflection.BindingFlags"},
                false,
                M(GetField)}}}});
        return resolver.Resolve(ns, type, name, result, parameters, kind);
    }
    DeclaredType ReadObjectType(void*) override {
        return {"System.Reflection",
                "RuntimeFieldInfo",
                {{"GetValue", "System.Object", {"System.Object"}, false, M(ReadField)},
                 {"SetValue",
                  "System.Void",
                  {"System.Object", "System.Object", "System.Reflection.BindingFlags", "System.Reflection.Binder",
                   "System.Globalization.CultureInfo"},
                  false,
                  M(WriteField)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override {
        if (fields_fail)
            throw std::runtime_error("layout unavailable");
        if (boxed.count(object))
            return {{"UnityEngine", "Vector3", "x", "System.Single", offsetof(Box, vector), 6, true},
                    {"UnityEngine", "Vector3", "y", "System.Single", offsetof(Box, vector) + 4, 6, true},
                    {"UnityEngine", "Vector3", "z", "System.Single", offsetof(Box, vector) + 8, 6, true}};
        if (object == &point)
            return {{"RPG.Client", "AttachPoint", "PointTransform", "UnityEngine.Transform", offsetof(Point, transform),
                     6, true}};
        return {
            {"", "AttachPointSet", "TargetScale", "UnityEngine.Vector3", offsetof(Component, target), 6, true},
            {"", "AttachPointSet", "AttachPoint", "RPG.Client.AttachPoint", offsetof(Component, point), 6, true},
            {"", "AttachPointSet", "ParentConstraint", "UnityEngine.Transform", offsetof(Component, parent), 6, true},
            {"", "AttachPointSet", "IsDestroy", "System.Boolean", offsetof(Component, destroyed), 6, true}};
    }
    ObjectResult NewStringUtf8(const char* text) override {
        strings.push_back(std::make_unique<std::string>(text));
        return {{true, 0}, strings.back().get()};
    }
    ObjectResult ManagedType(const char* ns, const char* name) override {
        ++type_reads;
        const auto key = std::string(ns) + "." + name;
        if (type_fail && key == ".AttachPointSet")
            return {};
        auto& result = types[key];
        if (!result)
            result = std::make_unique<Type>(Type{key});
        return {{true, 0}, result.get()};
    }
    ObjectResult AllocateObject(const char* ns, const char* name) override {
        Check(std::string(ns) == "UnityEngine" && std::string(name) == "Vector3");
        return {{true, 0}, NewBox()};
    }
};
} // namespace
int main() {
    try {
        // This fixture exercises production native adaptation, not game rendering.
        Provider provider;
        ReferenceMethods refs{M(Keep), M(Target), M(Free), {}, {}};
        {
            const auto reads = type_reads;
            auto deferred =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            Check(roots.empty() && type_reads == reads);
            type_fail = true;
            bool failed = false;
            try {
                deferred->Set(1, "Arrow", {0, 0, 0});
            } catch (const std::exception&) {
                failed = true;
            }
            type_fail = false;
            Check(failed && roots.size() == 1 && registered == 0);
            Check(deferred->Close() && roots.empty()); // the retained model survives failed Type preparation
        }
        {
            auto replaced =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            const auto reads = type_reads;
            on_keep = [] { current = false; };
            bool failed = false;
            try {
                replaced->Set(1, "Arrow", {0, 0, 0});
            } catch (const std::exception&) {
                failed = true;
            }
            Check(failed && roots.size() == 1 && type_reads == reads && registered == 0);
            Check(replaced->Close() && roots.empty());
            current = true;
        }
        {
            auto expired =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            const auto reads = type_reads;
            current = false;
            bool failed = false;
            try {
                expired->Set(1, "Arrow", {0, 0, 0});
            } catch (const std::exception&) {
                failed = true;
            }
            Check(failed && roots.empty() && type_reads == reads && registered == 0);
            Check(expired->Close());
            current = true;
        }
        {
            auto preparing =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            const auto frees = free_calls;
            bool observed = false;
            on_keep = [&] {
                observed = true;
                Check(!preparing->Close() && !preparing->Release(1) && free_calls == frees);
            };
            preparing->Set(1, "Arrow", {0, 0, 0});
            Check(observed && registered == 1 && !roots.empty());
            Check(preparing->Close() && roots.empty() && registered == 0);
            point.local = {1, 1, 1};
            point.persistent.reset();
        }
        {
            auto writing =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            bool observed = false;
            on_write = [&] {
                observed = true;
                const auto frees = free_calls;
                bool nested_rejected = false;
                try {
                    writing->Set(2, "Arrow", {2, 2, 2});
                } catch (const std::logic_error&) {
                    nested_rejected = true;
                }
                Check(nested_rejected);
                Check(!writing->Close() && free_calls == frees);
            };
            writing->Set(1, "Arrow", {0, 0, 0});
            Check(observed && registered == 1);
            Check(writing->Close() && roots.empty() && registered == 0);
            point.local = {1, 1, 1};
            point.persistent.reset();
        }
        for (bool existing : {false, true}) {
            original = {};
            original.point = &point;
            original.target = {2, 2, 2};
            original.origin = {1, 1, 1};
            selected = existing ? &original : nullptr;
            registered = existing ? 1 : 0;
            point.local = {2, 2, 2};
            point.persistent = point.local;
            auto reading =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            if (existing) {
                reading->Set(1, "Arrow", {0, 0, 0});
                original.target = {3, 3, 3}; // a later input must capture this new official baseline
            }
            const auto reads = local_reads;
            on_persistent_read = [] { on_keep = [] { current = false; }; };
            bool failed = false;
            try {
                reading->Set(2, "Arrow", {0, 0, 0});
            } catch (const std::exception&) {
                failed = true;
            }
            Check(failed && !current && local_reads == reads);
            Check(reading->Close() && roots.empty() && local_reads == reads);
            current = true;
            selected = nullptr;
            registered = 0;
            point.local = {1, 1, 1};
            point.persistent.reset();
        }
        destroyed = 0;
        auto session =
            detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
        // PointTransform is a reference field, not inline Transform storage.
        session->Set(1, "Arrow", {0, 0, 0});
        Check(point.local == V({1, 1, 1}) && registered == 1);
        Tick(selected, nullptr);
        Check(point.local == V({0, 0, 0}));
        point.local = {7, 7, 7};
        Tick(selected, nullptr);
        Check(point.local == V({0, 0, 0}));
        Check(session->Release(1) && point.local == V({1, 1, 1}) && !point.persistent && registered == 0 &&
              destroyed == 1);
        const auto baseline = roots.size();
        for (int i = 0; i < 8; ++i) {
            session->Set(2, "Arrow", {0, 0, 0});
            Check(session->Release(2));
        }
        Check(roots.size() == baseline); // per-contribution boxed snapshots are released
        original = {};
        original.point = &point;
        original.target = {2, 2, 2};
        original.origin = {1, 1, 1};
        original.parent = &go;
        selected = &original;
        registered = 1;
        point.local = {2, 2, 2};
        point.persistent = point.local;
        session->Set(3, "Arrow", {0, 0, 0});
        Check(original.target == V({0, 0, 0}) && original.parent == &go);
        Check(session->Release(3) && original.target == V({2, 2, 2}) && original.parent == &go && registered == 1);
        session->Set(4, "Arrow", {0, 0, 0});
        original.target = {3, 3, 3};
        const auto before = ticks;
        Check(session->Release(4) && original.target == V({3, 3, 3}) && ticks == before);
        session->Set(7, "Arrow", {0, 0, 0});
        session->Set(8, "Arrow", {2, 2, 2});
        original.target = {4, 4, 4};
        Check(session->Release(8) && original.target == V({4, 4, 4}));
        Check(session->Release(7) && original.target == V({4, 4, 4}));
        session->Set(9, "Arrow", {0, 0, 0});
        original.target = {5, 5, 5};
        session->Set(10, "Arrow", {2, 2, 2});
        Check(session->Release(9) && session->Release(10) && original.target == V({5, 5, 5}));
        original.target = {-1, -1, -1};
        point.local = {6, 6, 6};
        point.persistent.reset();
        session->Set(11, "Arrow", {0, 0, 0});
        Check(session->Release(11) && point.local == V({6, 6, 6}) && !point.persistent);
        session->Set(12, "Arrow", {0, 0, 0});
        SelfDestroy(&original, nullptr);
        Destroy(&original, 0, nullptr);
        session->Set(13, "Arrow", {2, 2, 2});
        Tick(selected, nullptr);
        Check(selected == &added && point.local == V({2, 2, 2}));
        Check(session->Release(13) && registered == 0 && session->Release(12));
        fields_fail = true;
        bool failed = false;
        try {
            session->Set(5, "Arrow", {0, 0, 0});
        } catch (const std::exception&) {
            failed = true;
        }
        fields_fail = false;
        Check(failed && session->Release(5));
        session->Set(6, "Arrow", {0, 0, 0});
        current = false;
        const auto ended = ticks;
        Check(session->Close() && roots.empty() && ticks == ended && session->Close());
        current = true;
        {
            auto unknown =
                detail::NativeAttachmentScale(provider, provider, provider, refs, &model, [] { return current; });
            unknown->Set(1, "Arrow", {0, 0, 0});
            current = false;
            free_fault = true;
            Check(!unknown->Close());
            const auto frees = free_calls;
            free_fault = false;
            Check(!unknown->Close() && free_calls == frees); // uncertain frees are not repeated
        }
        roots.clear(); // fixture handles with unknown release; no production retry
        std::cout << "native attachment scale lazy inputs, reentrant close, apply/restore and ownership passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
