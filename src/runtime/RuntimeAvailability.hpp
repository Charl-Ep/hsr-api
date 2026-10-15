#pragma once
#include <hksr/runtime/RuntimeAvailability.hpp>
#include <Windows.h>
#include <cstddef>

namespace hksr::sdk_v1::detail {
inline RuntimeAvailability ObserveDomainRaw(void* domain_get,void* assemblies_get) {
    if(!domain_get || !assemblies_get) return RuntimeAvailability::WaitingTable;
    __try {
        auto* domain=reinterpret_cast<void*(__fastcall*)()>(domain_get)();
        if(!domain) return RuntimeAvailability::WaitingDomain;
        std::size_t count=0;
        auto** assemblies=reinterpret_cast<void**(__fastcall*)(void*,std::size_t*)>(assemblies_get)(domain,&count);
        return assemblies && count?RuntimeAvailability::Available:RuntimeAvailability::WaitingAssemblies;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return RuntimeAvailability::Unsupported;
    }
}
inline RuntimeAvailabilityResult ObserveDomain(void* domain_get,void* assemblies_get) {
    const auto state=ObserveDomainRaw(domain_get,assemblies_get);
    return {state,state==RuntimeAvailability::Unsupported?"bootstrap readiness call faulted":""};
}
}
