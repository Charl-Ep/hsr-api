#include <hksr/unity/OwnedUnityObjects.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <map>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr::api;
struct Object { bool alive = true; unsigned requests = 0; } first, second;
int metadata, sequence = 0;
std::map<int, void*> roots;
bool destroy_fault = false, alive_fault = false, target_fault = false;
void Check(bool ok) { if (!ok) throw std::runtime_error("Unity lifetime test failed"); }
int __fastcall Keep(void* object, int zero, int kind, void*) { Check(!zero && kind == 2); roots[++sequence] = object; return sequence; }
void* __fastcall Target(int id, void*) { if (target_fault) RaiseException(0xE0530001, 0, 0, nullptr); return roots.at(id); }
void __fastcall Free(int id, void*) { roots.erase(id); }
bool __fastcall Alive(Object* object, void* info) {
    Check(info == &metadata);
    if (alive_fault && object == &first) RaiseException(0xE0530002, 0, 0, nullptr);
    return object->alive;
}
void __fastcall Destroy(Object* object, void* info) {
    Check(info == &metadata); ++object->requests;
    if (destroy_fault) RaiseException(0xE0530003, 0, 0, nullptr);
    // It is scheduled, not immediately destroyed.
}
template<class T> Method M(T function) { return {reinterpret_cast<void*>(function), &metadata}; }
}
void TestUnityLifetime() {
    MetadataResolver resolver({{"UnityEngine", "Object", {
        {"IsNativeObjectAlive", "System.Boolean", {"UnityEngine.Object"}, true, M(Alive)},
        {"Destroy", "System.Void", {"UnityEngine.Object"}, true, M(Destroy)}}}});
    const auto methods = ResolveUnityLifetimeMethods(resolver);
    const ReferenceMethods references{M(Keep), M(Target), M(Free), {}, {}};
    OwnedUnityObjects owned(methods, references);
    const auto a = owned.Adopt(&first), b = owned.Adopt(&second);
    Check(roots.size() == 2 && owned.Status(a).root.ready);
    bool duplicate = false; try { owned.Adopt(&first); } catch (const std::logic_error&) { duplicate = true; }
    Check(duplicate && roots.size() == 2);
    alive_fault = true;
    const auto independent = owned.CloseAll();
    Check(independent.size() == 2 && first.requests == 0 && second.requests == 1 && roots.size() == 2);
    alive_fault = false; destroy_fault = true;
    Check(owned.Close(a).destroy.exception == 0xE0530003 && first.requests == 1);
    destroy_fault = false; owned.CloseAll();
    Check(first.requests == 1 && second.requests == 1 && roots.size() == 2);
    first.alive = false; target_fault = true;
    Check(!owned.Close(a).complete && roots.size() == 2);
    target_fault = false;
    Check(owned.Close(a).complete && roots.size() == 1);
    second.alive = false;
    Check(owned.Close(b).complete && roots.empty());
    owned.Forget(a); owned.Forget(b);
    {
        first.alive = true;
        const auto delegated = owned.Adopt(&first);
        Check(!owned.ObserveEnd(delegated).complete && first.requests == 1);
        first.alive = false;
        Check(owned.ObserveEnd(delegated).complete && first.requests == 1 && roots.empty());
        owned.Forget(delegated);
    }
    {
        first.alive = false;
        const auto dead = owned.Adopt(&first);
        Check(owned.Close(dead).complete && first.requests == 1 && roots.empty());
        owned.Forget(dead);
    }
    std::cout << "Owned Unity lifetime tests passed: delayed destruction, independent outcomes and no repeated Destroy.\n";
}
