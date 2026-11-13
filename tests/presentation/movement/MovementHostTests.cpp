#include <hksr/presentation/movement/OwnedMovementHost.hpp>
#include <hksr/managed/GenericFactory.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
#include <functional>

using namespace hksr::api;
namespace {
void Check(bool value) {
    if (!value)
        throw std::runtime_error("movement host check failed");
}
int handle = 0, declaring = 1, argument = 2, type = 3, text = 4, definition = 5, closed_method = 6, entity = 7,
    component = 8, replacement = 9;
std::map<int, void*> roots;
std::vector<void*> generic_args{&argument}, input;
int find_count = 0, close_count = 0, invoke_count = 0, argument_reads = 0;
bool generic = true, open = false, invoke_fault = false;
bool free_fault = false;
int free_calls = 0;
std::function<void()> on_keep, on_definition, on_invoke, on_free;
int __fastcall Keep(void* p, int, int kind, void*) {
    Check(kind == 2);
    const auto id = ++handle;
    roots[id] = p;
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
    if (on_free) {
        auto callback = std::move(on_free);
        on_free = {};
        callback();
    }
    if (free_fault)
        RaiseException(0xe0470012, 0, 0, nullptr);
    roots.erase(id);
}
template <class F> Method M(F f) {
    return {reinterpret_cast<void*>(f), nullptr};
}
ReferenceMethods Refs() {
    return {M(Keep), M(Target), M(Free), {}, {}};
}
void* __fastcall Find(void* t, void* n, void*) {
    Check(t == &declaring && n == &text);
    ++find_count;
    return &definition;
}
bool __fastcall Definition(void* p, void*) {
    Check(p == &definition);
    if (on_definition) {
        auto callback = std::move(on_definition);
        on_definition = {};
        callback();
    }
    return generic;
}
void* __fastcall Arguments(void* p, void*) {
    Check(p == &definition);
    ++argument_reads;
    return &generic_args;
}
void* __fastcall Make(void* p, void* a, void*) {
    Check(p == &definition && a == &input && input == generic_args);
    ++close_count;
    return &closed_method;
}
bool __fastcall Open(void* p, void*) {
    Check(p == &closed_method);
    return open;
}
void* __fastcall Invoke(void* method, void* receiver, void* args, void*) {
    Check(method == &closed_method && receiver == &entity && !args);
    ++invoke_count;
    if (on_invoke) {
        auto callback = std::move(on_invoke);
        on_invoke = {};
        callback();
    }
    if (invoke_fault)
        RaiseException(0xe0470011, 0, 0, nullptr);
    return &component;
}
void* __fastcall CreateArray(void* element, int count, void*) {
    Check(element == &type && count == 1);
    input.assign(1, nullptr);
    return &input;
}
int __fastcall Length(void* p, void*) {
    return static_cast<int>(static_cast<std::vector<void*>*>(p)->size());
}
void __fastcall Set(void* p, void* item, int i, void*) {
    static_cast<std::vector<void*>*>(p)->at(i) = item;
}
struct Objects : ManagedObjects {
    ObjectResult NewStringUtf8(const char* value) override {
        Check(std::string(value) == "CreateComponent");
        return {{true, 0}, &text};
    }
    ObjectResult ManagedType(const char* ns, const char* name) override {
        Check(std::string(ns) == "System" && std::string(name) == "Type");
        return {{true, 0}, &type};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("raw allocation is not component registration");
    }
};
void Reflection() {
    Objects objects;
    GenericFactoryMethods methods{M(Find), M(Definition), M(Arguments), M(Make), M(Open), M(Invoke)};
    ArrayMethods arrays;
    arrays.create = M(CreateArray);
    arrays.length = M(Length);
    arrays.set = M(Set);
    GenericFactory factory(methods, Refs(), objects, ManagedArrays(arrays));
    Check(factory.Prepare(&declaring, "CreateComponent", &argument));
    Check(factory.Prepare(&declaring, "CreateComponent", &argument));
    Check(!factory.Prepare(&declaring, "Other", &argument));
    Check(factory.Invoke(&entity).object == &component && factory.Invoke(&entity).call.returned);
    Check(find_count == 1 && close_count == 1 && invoke_count == 2);
    invoke_fault = true;
    Check(!factory.Invoke(&entity).call.returned);
    invoke_fault = false;
    Check(factory.Close() && roots.empty() && !factory.Invoke(&entity).call.returned);
    generic = false;
    GenericFactory wrong(methods, Refs(), objects, ManagedArrays(arrays));
    Check(!wrong.Prepare(&declaring, "CreateComponent", &argument));
    Check(wrong.Close() && roots.empty());
    generic = true;
    open = true;
    GenericFactory unclosed(methods, Refs(), objects, ManagedArrays(arrays));
    Check(!unclosed.Prepare(&declaring, "CreateComponent", &argument));
    Check(unclosed.Close() && roots.empty());
    open = false;
    {
        GenericFactory preparing(methods, Refs(), objects, ManagedArrays(arrays));
        bool close_completed = false;
        const auto finds = find_count;
        const auto frees = free_calls;
        on_keep = [&] { close_completed = preparing.Close(); };
        const auto prepared = preparing.Prepare(&declaring, "CreateComponent", &argument);
        if (close_completed || prepared || find_count != finds || free_calls != frees)
            throw std::runtime_error("generic preparation continued after reentrant Close");
        Check(!roots.empty() && preparing.Close() && roots.empty());
    }
    {
        GenericFactory preparing(methods, Refs(), objects, ManagedArrays(arrays));
        bool close_completed = false;
        const auto reads = argument_reads;
        on_definition = [&] { close_completed = preparing.Close(); };
        Check(!preparing.Prepare(&declaring, "CreateComponent", &argument));
        Check(!close_completed && argument_reads == reads && !roots.empty());
        Check(preparing.Close() && roots.empty());
    }
    {
        GenericFactory invoking(methods, Refs(), objects, ManagedArrays(arrays));
        Check(invoking.Prepare(&declaring, "CreateComponent", &argument));
        bool close_completed = false;
        const auto frees = free_calls;
        on_invoke = [&] {
            const auto nested = invoking.Invoke(&entity);
            Check(nested.call.returned && nested.object == &component);
            close_completed = invoking.Close();
        };
        const auto created = invoking.Invoke(&entity);
        Check(created.call.returned && created.object == &component);
        Check(!close_completed && free_calls == frees && !roots.empty());
        Check(invoking.Close() && roots.empty());
    }
    {
        GenericFactory releasing(methods, Refs(), objects, ManagedArrays(arrays));
        Check(releasing.Prepare(&declaring, "CreateComponent", &argument));
        bool nested_closed = true;
        const auto frees = free_calls;
        const auto count = roots.size();
        on_free = [&] { nested_closed = releasing.Close(); };
        Check(releasing.Close() && !nested_closed && roots.empty());
        Check(free_calls - frees == static_cast<int>(count));
    }
    {
        GenericFactory unknown(methods, Refs(), objects, ManagedArrays(arrays));
        Check(unknown.Prepare(&declaring, "CreateComponent", &argument));
        free_fault = true;
        Check(!unknown.Close());
        const auto frees = free_calls;
        free_fault = false;
        Check(!unknown.Close() && free_calls == frees && !unknown.Invoke(&entity).call.returned);
    }
    roots.clear(); // fixture handles with unknown release; no production retry
}
struct Host {
    void* registered = nullptr;
    bool live = true, current = true, root = true, callback = true, pending = false, active = false;
    bool init_ok = true, create_ok = true, remove_ok = true, pause_ok = true;
    int created = 0, initialized = 0, removed = 0, root_writes = 0, callback_writes = 0;
    MovementHostServices Services() {
        MovementHostServices s;
        s.references = Refs();
        s.current = [this] { return current; };
        s.alive = [this] { return live; };
        s.lookup = [this] { return ObjectResult{{true, 0}, registered}; };
        s.create = [this] {
            ++created;
            registered = &component;
            return ObjectResult{{create_ok, 0}, create_ok ? &component : nullptr};
        };
        s.initialize = [this](void* c) {
            Check(c == &component);
            ++initialized;
            return CallResult{init_ok, 0};
        };
        s.remove = [this](void* c) {
            Check(c == &component && registered == c);
            ++removed;
            if (remove_ok)
                registered = nullptr;
            return CallResult{remove_ok, 0};
        };
        s.observe = [this](void*) {
            AnimationMovementObservation o;
            o.read.returned = true;
            o.owner = &entity;
            o.transform = &type;
            o.animator_source = &argument;
            if (active)
                o.data = &text;
            return o;
        };
        s.read_root_motion = [this] { return Observed<bool>{root, {true, 0}}; };
        s.write_root_motion = [this](bool v) {
            ++root_writes;
            root = v;
            return CallResult{v || pause_ok, 0};
        };
        s.read_pending_motion = [this] { return Observed<bool>{pending, {true, 0}}; };
        s.read_callback = [this](void*) { return Observed<bool>{callback, {true, 0}}; };
        s.write_callback = [this](void* c, bool v) {
            Check(c == &component);
            ++callback_writes;
            callback = v;
            return CallResult{true, 0};
        };
        return s;
    }
    EntityToken Token() {
        return {reinterpret_cast<GameEntity*>(&entity), 1};
    }
};
void Lifecycle() {
    {
        Host h;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(x.Prepare() && x.Ready());
        Check(!h.root && !h.callback && h.created == 1 && h.initialized == 1);
        Check(x.Prepare() && h.created == 1);
        h.active = true;
        Check(!x.Close() && !h.root && h.removed == 0);
        h.active = false;
        Check(x.Close() && h.root && h.removed == 1 && roots.empty());
        Check(!x.Prepare() && !x.Ready() && !x.Component());
    }
    {
        Host h;
        h.registered = &component;
        h.pending = true;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(x.Prepare() && !x.Ready() && x.Status().waiting && h.created == 0 && h.initialized == 0);
        h.pending = false;
        Check(x.Ready());
        Check(x.Close() && h.root && h.callback && h.removed == 0 && roots.empty());
    }
    {
        Host h;
        h.init_ok = false;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(!x.Prepare() && x.Status().closed && h.removed == 1 && h.root_writes == 0 && roots.empty());
    }
    {
        Host h;
        h.create_ok = false;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(!x.Prepare() && x.Status().closed && h.removed == 1 && h.initialized == 0 && roots.empty());
    }
    {
        Host h;
        h.pause_ok = false;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(!x.Prepare() && x.Status().closed && h.root && h.removed == 1 && roots.empty());
    }
    {
        Host h;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(x.Prepare());
        h.registered = &replacement;
        Check(x.Close() && x.Status().component_replaced && h.removed == 0 && h.root && roots.empty());
    }
    {
        Host h;
        h.registered = &component;
        h.active = true;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(!x.Prepare() && h.removed == 0 && h.root_writes == 0 && roots.empty());
    }
    {
        Host h;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(x.Prepare());
        h.live = false;
        Check(x.Close() && x.Status().owner_lost && h.removed == 0 && h.root_writes == 1 && roots.empty());
    }
    {
        Host h;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(x.Prepare());
        h.remove_ok = false;
        Check(!x.Close() && !x.Close() && h.removed == 1 && !roots.empty());
        h.live = false;
        Check(x.Close() && roots.empty());
    }
    {
        Host h;
        h.current = false;
        OwnedMovementHost x(h.Services(), h.Token());
        Check(!x.Prepare() && h.created == 0 && h.root_writes == 0 && roots.empty());
    }
}
}
int main() {
    try {
        Reflection();
        Lifecycle();
        std::cout << "Movement host construction, rollback, drain and isolation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
