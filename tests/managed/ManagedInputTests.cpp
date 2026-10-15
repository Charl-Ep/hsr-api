#include <hksr/native/ObjectAccess.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace hksr::api;
int klass, object, type, reflection_type;
int allocations = 0, frees = 0, next_handle = 0;
bool creation_fault = false, get_fault = false, free_fault = false, retain_fault = false;
std::map<int, void*> roots;
void Check(bool ok) { if (!ok) throw std::runtime_error("managed input test failed"); }
void* __fastcall String(const char* value) {
    Check(std::string(value) == "藿藿");
    if (creation_fault) RaiseException(0xE0470001, 0, 0, nullptr);
    return &object;
}
void* __fastcall Allocate(void* value) { Check(value == &klass); ++allocations; return &object; }
void* __fastcall ClassType(void* value) { Check(value == &klass); return &type; }
void* __fastcall TypeObject(void* value) { Check(value == &type); return &reflection_type; }
int __fastcall Retain(void* value, int handle, int kind, void*) {
    Check(!handle && kind == 2);
    if (retain_fault) RaiseException(0xE0470002, 0, 0, nullptr);
    roots[++next_handle] = value; return next_handle;
}
void* __fastcall Get(int handle, void*) {
    if (get_fault) RaiseException(0xE0470003, 0, 0, nullptr);
    return roots.at(handle);
}
void __fastcall Free(int handle, void*) {
    ++frees;
    if (free_fault) RaiseException(0xE0470004, 0, 0, nullptr);
    roots.erase(handle);
}
template<class T> void* P(T f) { return reinterpret_cast<void*>(f); }
struct Resolver : MethodResolver {
    unsigned calls = 0;
    Method Resolve(const char* space, const char* owner, const char* name, const char*, std::initializer_list<const char*>, MethodKind kind) override {
        Check(std::string(space) == "System.Runtime.InteropServices" && std::string(owner) == "GCHandle" && kind == MethodKind::Static);
        ++calls;
        if (std::string(name) == "GetTargetHandle") return {P(Retain), nullptr};
        if (std::string(name) == "GetTarget") return {P(Get), nullptr};
        if (std::string(name) == "FreeHandle") return {P(Free), nullptr};
        throw std::runtime_error("unrelated dependency");
    }
};
}
void TestManagedInputs() {
    native::ObjectFunctions functions{P(String), P(Allocate), P(ClassType), P(TypeObject)};
    Check(native::NewString(functions, "藿藿").object == &object);
    Check(native::AllocateObject(functions, &klass).object == &object && allocations == 1);
    Check(native::TypeObject(functions, &klass).object == &reflection_type);
    Check(!native::NewString(functions, nullptr).call.returned);
    creation_fault = true;
    const auto failed = native::NewString(functions, "藿藿");
    Check(!failed.call.returned && failed.call.exception == 0xE0470001);
    creation_fault = false;
    functions.object_new = nullptr; functions.class_get_type = nullptr;
    Check(native::NewString(functions, "藿藿").call.returned);
    Check(!native::AllocateObject(functions, &klass).call.returned && !native::TypeObject(functions, &klass).call.returned);
    Resolver resolver;
    ManagedRoots held(ResolveGcMethods(resolver)); Check(resolver.calls == 3);
    auto a = held.Keep(&object), b = held.Keep(&object);
    Check(a != b && held.Target(a).object == &object && roots.size() == 2);
    Check(held.Release(a).complete && held.Release(a).complete && frees == 1);
    held.Forget(a); Check(held.Target(b).object == &object);
    get_fault = true;
    Check(!held.Target(b).call.returned); // read error must not leak ownership
    auto c = held.Keep(&object); Check(held.Status(c).complete && !held.Status(c).ready);
    get_fault = false; held.Forget(c);
    Check(held.Release(b).complete); held.Forget(b); Check(roots.empty());
    auto d = held.Keep(&object); free_fault = true;
    Check(!held.Release(d).complete);
    const auto before = frees; free_fault = false;
    held.Release(d); Check(frees == before);
    bool rejected = false; try { held.Forget(d); } catch (const std::logic_error&) { rejected = true; } Check(rejected);
    retain_fault = true;
    auto e = held.Keep(&object); Check(!held.Status(e).complete && !held.Status(e).ready);
    retain_fault = false;
    const auto last = frees;
    { ManagedRoots short_lived(ResolveGcMethods(resolver)); short_lived.Keep(&object); }
    Check(frees == last); // destruction is not an arbitrary-thread engine call
    std::cout << "Managed input tests passed: native ABI, independent binding and explicit GC ownership.\n";
}
