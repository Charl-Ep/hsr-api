#pragma once
#include <hksr/native/MetadataReader.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <Windows.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace hksr::test {
using namespace hksr::api;
using namespace hksr::sdk_v1::native;
inline void Check(bool value) { if (!value) throw std::runtime_error("native metadata reader test failed"); }
struct FakeMethod {
    void* owner;
    void* code;
    std::array<unsigned char, 26> padding{};
    std::uint16_t flags = 0;
    const char* name;
    const char* result;
    std::vector<const char*> parameters;
};
static_assert(offsetof(FakeMethod, code) == 8 && offsetof(FakeMethod, flags) == 42);
int domain, image, executable;
std::vector<FakeMethod*> methods;
std::vector<void*> classes;
int name_acquisitions, name_releases;
bool invalid_string, repeated_method, absent_domain, repeated_field, invalid_assembly_span;
int parent_class;
bool parent_cycle;
void* parameter_class = nullptr;
inline void* __fastcall ClassFromType(void* type) { Check(type != nullptr); return parameter_class; }
struct FakeField { const char* name; const char* type; std::size_t offset; std::uint32_t flags=6; };
FakeField child_field{"Name", "System.String", 24}, base_field{"OnStart", "RPG.GameCore.TaskList", 48};
inline void* __fastcall Domain() { return absent_domain ? nullptr : &domain; }
inline void** __fastcall Assemblies(void* value, std::size_t* count) {
    if(invalid_assembly_span) {*count=static_cast<std::size_t>(-1);return reinterpret_cast<void**>(1);}
    Check(value == &domain); static void* objects[] = {&image}; *count = 1; return objects;
}
inline void* __fastcall Image(void* value) { Check(value == &image); return value; }
inline std::size_t __fastcall ClassCount(void* value) { Check(value == &image); return classes.size(); }
inline void* __fastcall ClassAt(void* value, std::size_t index) { Check(value == &image); return classes.at(index); }
inline const char* __fastcall ClassName(void* type) { return type == &parent_class ? "Base" : "Model"; }
inline const char* __fastcall ClassSpace(void*) { return "Game"; }
inline void* __fastcall NextMethod(void*, void** iterator) {
    const auto index = reinterpret_cast<std::uintptr_t>(*iterator);
    *iterator = reinterpret_cast<void*>(index + 1);
    if (repeated_method) return methods.at(0);
    return index < methods.size() ? methods[index] : nullptr;
}
inline const char* __fastcall Name(FakeMethod* value) { return value->name; }
inline void* __fastcall Returns(FakeMethod* value) { return const_cast<char*>(value->result); }
inline unsigned __fastcall ParamCount(FakeMethod* value) { return static_cast<unsigned>(value->parameters.size()); }
inline void* __fastcall Param(FakeMethod* value, unsigned index) { return const_cast<char*>(value->parameters.at(index)); }
inline void* __fastcall TypeName(const char* value) {
    ++name_acquisitions;
    return invalid_string ? reinterpret_cast<void*>(1) : _strdup(value);
}
inline void __fastcall Release(void* value) {
    ++name_releases;
    if (value != reinterpret_cast<void*>(1)) std::free(value);
}
inline void* __fastcall Parent(void* type, void* unused) {
    Check(!unused);
    if (type == &parent_class) return parent_cycle ? &parent_class : nullptr;
    return &parent_class;
}
inline void* __fastcall Field(void* type, void** iterator) {
    if(repeated_field) return &child_field;
    if (*iterator) return nullptr;
    *iterator = reinterpret_cast<void*>(1);
    return type == &parent_class ? &base_field : &child_field;
}
inline const char* __fastcall FieldName(FakeField* field) { return field->name; }
inline void* __fastcall FieldType(FakeField* field) { return const_cast<char*>(field->type); }
inline std::size_t __fastcall FieldOffset(FakeField* field) { return field->offset; }
inline std::uint32_t __fastcall FieldFlags(FakeField* field) { return field->flags; }
template<class F> void* Pointer(F function) { return reinterpret_cast<void*>(function); }
}

