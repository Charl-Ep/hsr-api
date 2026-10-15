#include "AnimatorLayout.hpp"
#include "AnimatorTable.hpp"
#include "../runtime/RuntimeProfile.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>

namespace hksr::sdk_v1::detail {
namespace {
const TableRead memory=[](std::uintptr_t address,void* output,std::size_t size) {return native::CopyMemoryChecked(output,reinterpret_cast<void*>(address),size);};
void Require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
std::uintptr_t Jump(std::uintptr_t address) {
    Require(TableValue<unsigned char>(memory,address)==0xe9,"expected native state writer jump");
    return TableAdd(TableAdd(address,5),TableValue<std::int32_t>(memory,TableAdd(address,1)));
}
void Bytes(std::uintptr_t address,std::initializer_list<unsigned char> expected) {
    std::vector<unsigned char> actual(expected.size());
    Require(memory(address,actual.data(),actual.size()) && std::equal(actual.begin(),actual.end(),expected.begin()),"native state writer relation differs");
}
}
AnimatorLayout LocateAnimatorLayout(Method state_info,RuntimeVersion version) {
    // Narrow, named-entry relation. No module-wide pattern scan, retained RVA,
    // guessed hash names or fingerprint file. This layout is currently 4.6 only.
    Require(version==RuntimeVersion::Hsr460,"Animator layout is qualified only for 4.6");
    const auto unity=UnityModule(RuntimeProfileFor(version));
    const auto entry=reinterpret_cast<std::uintptr_t>(state_info.code);
    Bytes(entry,{0x48,0x8b,0x05});Bytes(entry+7,{0x48,0xff,0xe0});
    const auto slot=TableAdd(entry+7,TableValue<std::int32_t>(memory,entry+3));
    const auto wrapper=TableValue<std::uintptr_t>(memory,slot);
    Require(unity.Executable(reinterpret_cast<void*>(wrapper)),"state icall target is outside current Unity executable");
    Bytes(wrapper,{0x48,0x83,0xec,0x38,0x48,0x85,0xc9,0x74,0x15,0x48,0x8b,0x41,0x10,0x48,0x85,0xc0,0x74,0x0c,0x48,0x8b,0xc8,0x48,0x83,0xc4,0x38});
    const auto bridge=Jump(wrapper+25);
    Require(unity.Executable(reinterpret_cast<void*>(bridge)),"state bridge outside Unity");
    Bytes(bridge,{0x48,0x83,0xb9});Bytes(bridge+7,{0x00,0x76,0x19,0x48,0x83,0xb9});
    Bytes(bridge+17,{0x00,0x76,0x0f,0x48,0x8b,0x89});Bytes(bridge+27,{0x48,0x8b,0x09});
    AnimatorLayout out{TableValue<std::uint32_t>(memory,bridge+3),TableValue<std::uint32_t>(memory,bridge+13),TableValue<std::uint32_t>(memory,bridge+23),0};
    const auto writer=Jump(bridge+30);
    Require(unity.Executable(reinterpret_cast<void*>(writer)),"state writer outside Unity");
    Bytes(writer+0x2f,{0x4c,0x8b,0x86});out.constant=TableValue<std::uint32_t>(memory,writer+0x32);
    // Full layer -> machine -> state traversal and identity/loop loads. These
    // are semantic layout checks, not a self-computed source/DLL hash.
    Bytes(writer+0x42,{0x4d,0x8d,0x50,0x08,0x4d,0x85,0xc0,0x74,0x2c,0x49,0x8b,0x12,0x49,0x03,0xd1,0x49,0x03,0xd2,0x48,0x8b,0x02,0x8b,0x0c,0x10});
    Bytes(writer+0x5a,{0x49,0x8b,0x40,0x18});
    Bytes(writer+0xe6,{0x41,0x3b,0x03});
    Bytes(writer+0xf2,{0x49,0x8d,0x53,0x08});
    Bytes(writer+0x103,{0x8b,0x4c,0x01,0x30});
    Bytes(writer+0x113,{0x8b,0x4c,0x01,0x34});
    Bytes(writer+0x124,{0x8b,0x4c,0x01,0x38});
    Bytes(writer+0x1c1,{0x80,0x7c,0x01,0x5a,0x00});
    return out;
}
}
