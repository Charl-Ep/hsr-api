#pragma once
#include <hksr/native/UnityMetadataLocator.hpp>
#include <hksr/runtime/RuntimeAvailability.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <optional>

namespace hksr::sdk_v1::detail {
struct RuntimeInitializationLocation { std::uint32_t mutex_slot, count; };

// The root-init wrapper calls Runtime::Init. Its entry locks the initialization
// mutex BEFORE incrementing its count; the success exit releases that mutex.
// A populated domain/table alone can be observed during this critical section.
// Resolve from slot0, not a per-version address or a whole-module scan.
inline std::optional<RuntimeInitializationLocation> LocateRuntimeInitialization(
    std::uint32_t entry,std::size_t image_size,const native::ReadImageBytes& read,
    std::uintptr_t enter_cs,std::uintptr_t leave_cs) {
    auto relative=[&](std::uint32_t at,int offset,int length,const unsigned char* bytes) -> std::uint32_t {
        std::int32_t delta=0;std::memcpy(&delta,bytes+offset,4);
        const auto value=static_cast<std::int64_t>(at)+length+delta;
        return value>=0 && static_cast<std::uint64_t>(value)<image_size?static_cast<std::uint32_t>(value):0;
    };
    std::array<unsigned char,64> wrapper{};
    if(!read(entry,wrapper.data(),wrapper.size())) return {};
    std::optional<RuntimeInitializationLocation> found;
    for(std::size_t call=0;call+5<=wrapper.size();++call) {
        if(wrapper[call]!=0xe8) continue;
        const auto target=relative(entry+static_cast<std::uint32_t>(call),1,5,wrapper.data()+call);
        std::array<unsigned char,160> code{};
        if(!target || !read(target,code.data(),code.size())) continue;
        for(std::size_t i=0;i+47<=code.size();++i) {
            const auto* p=code.data()+i;
            if(p[0]!=0x48 || p[1]!=0x8b || p[2]!=0x0d || p[7]!=0xff || p[8]!=0x15 ||
               p[13]!=0x8b || p[14]!=0x05 || p[19]!=0x8d || p[20]!=0x48 || p[21]!=1 ||
               p[22]!=0x89 || p[23]!=0x0d || p[28]!=0x85 || p[29]!=0xc0 || p[30]!=0x7e ||
               p[32]!=0x48 || p[33]!=0x8b || p[34]!=0x0d || p[39]!=0xff || p[40]!=0x15 ||
               p[45]!=0xb0 || p[46]!=1) continue;
            const auto at=target+static_cast<std::uint32_t>(i);
            const auto mutex=relative(at,3,7,p),count=relative(at,15,19,p);
            if(!mutex || !count || mutex!=relative(at,35,39,p) || count!=relative(at,24,28,p)) continue;
            std::uintptr_t enter=0,leave=0;
            if(!read(relative(at,9,13,p),&enter,sizeof(enter)) ||
               !read(relative(at,41,45,p),&leave,sizeof(leave)) || enter!=enter_cs || leave!=leave_cs) continue;
            if(found && (found->mutex_slot!=mutex || found->count!=count)) return {};
            found=RuntimeInitializationLocation{mutex,count};
        }
    }
    return found;
}

inline RuntimeAvailability ObserveInitialization(CRITICAL_SECTION* const* slot,const int* count) {
    __try {
        auto* mutex=*slot;
        if(!mutex) return RuntimeAvailability::WaitingInitialization;
        // TryEnter is recursive: observing on the initializer itself must not
        // mistake its in-progress count for a completed initialization.
        if(mutex->OwningThread==reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(GetCurrentThreadId())))
            return RuntimeAvailability::WaitingInitialization;
        if(!TryEnterCriticalSection(mutex)) return RuntimeAvailability::WaitingInitialization;
        int initialized=0;
        __try {initialized=*count;}
        __finally {LeaveCriticalSection(mutex);}
        return initialized>0?RuntimeAvailability::Available:RuntimeAvailability::WaitingInitialization;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return RuntimeAvailability::Unsupported;}
}
}
