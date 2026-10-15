#include "../../src/entity/NpcInputConstruction.hpp"
#include <Windows.h>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
struct Level { std::array<unsigned char, 16> header{}; unsigned npc_id = 0; unsigned default_value = 0; } level;
int definition, row, label, metadata;
unsigned defaults = 0, constructors = 0, next_handle = 0, frees = 0;
bool ctor_fault = false, row_missing = false;
std::map<int, void*> roots;
void Check(bool ok) { if (!ok) throw std::runtime_error("private NPC input test failed"); }
void* __fastcall GetRow(std::uint32_t id, void* info) { Check(id == 1217 && info == &metadata); return row_missing ? nullptr : &row; }
void __fastcall Defaults(Level* value, void* info) { Check(value == &level && info == &metadata); ++defaults; value->npc_id = 0; value->default_value = 42; }
void __fastcall Constructor(void* self, void* owner, std::uint32_t server, std::uint32_t group, std::uint32_t instance,
    std::uint32_t id, void* name, Position3* position, Rotation4* rotation, Level* input, void* optional, void* info) {
    ++constructors;
    Check(self == &definition && !owner && !server && !group && !instance && id == 1217 && name == &label && !optional && info == &metadata);
    Check(position->x == 1.5f && position->y == -3 && position->z == 4);
    Check(rotation->x == 0 && rotation->y == 0.6f && rotation->z == 0 && rotation->w == 0.8f);
    Check(input == &level && input->npc_id == 1217 && input->default_value == 42 && roots.size() == 4);
    if (ctor_fault) RaiseException(0xE04A0001, 0, 0, nullptr);
}
int __fastcall Retain(void* object, int current, int kind, void*) { Check(!current && kind == 2); roots[static_cast<int>(++next_handle)] = object; return static_cast<int>(next_handle); }
void* __fastcall Target(int handle, void*) { return roots.at(handle); }
void __fastcall Free(int handle, void*) { ++frees; roots.erase(handle); }
template<class T> Method M(T function) { return {reinterpret_cast<void*>(function), &metadata}; }
}
void TestNpcInputs() {
    const ReferenceMethods methods{M(Retain), M(Target), M(Free), {}, {}};
    const NpcInputOperations operations{
        [] { return ObjectResult{{true,0}, &level}; },
        [] { return ObjectResult{{true,0}, &definition}; },
        [](const char* text) { Check(std::string(text) == "private npc"); return ObjectResult{{true,0}, &label}; },
        M(GetRow), M(Defaults), M(Constructor), offsetof(Level, npc_id)
    };
    NpcInputConstruction input(methods);
    input.Prepare(operations, 1217, "private npc", {1.5f,-3,4}, {0,0.6f,0,0.8f});
    Check(input.Status().ready && input.Status().phase == NpcInputPhase::Ready);
    Check(input.Definition() == &definition && input.Row() == &row && defaults == 1 && constructors == 1);
    Check(input.Release() && roots.empty() && frees == 4 && !input.Definition());
    Check(input.Release() && frees == 4);
    ctor_fault = true;
    NpcInputConstruction partial(methods);
    partial.Prepare(operations, 1217, "private npc", {1.5f,-3,4}, {0,0.6f,0,0.8f});
    Check(!partial.Status().ready && partial.Status().phase == NpcInputPhase::DefinitionConstructor);
    Check(partial.Status().last_call.exception == 0xE04A0001 && roots.size() == 4 && !partial.Definition());
    Check(partial.Release() && roots.empty()); ctor_fault = false;
    row_missing = true;
    NpcInputConstruction absent(methods);
    const auto before = defaults;
    absent.Prepare(operations, 1217, "private npc", {1.5f,-3,4}, {0,0.6f,0,0.8f});
    Check(absent.Status().phase == NpcInputPhase::Row && !absent.Status().ready && defaults == before && roots.empty());
    Check(absent.Release()); row_missing = false;
    std::cout << "Private NPC input tests passed: official constructor ABI, defaults, roots and partial failure receipts.\n";
}
