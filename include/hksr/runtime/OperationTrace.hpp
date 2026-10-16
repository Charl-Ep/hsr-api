#pragma once
#include "../Export.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
struct OperationTiming {
    std::uint32_t parent=0;
    std::string stage;
    std::uintptr_t native_target=0;
    std::uint64_t calls=0,inclusive_ns=0,self_ns=0,max_ns=0,failures=0;
    std::string native_module;
    std::uintptr_t native_rva=0;
};
struct OperationTraceReport {
    std::uint64_t subject=0,generation=0,frame=0,tick_ms=0,dropped=0;
    std::string source;
    std::vector<OperationTiming> rows;
};
// Opt-in, thread-local synchronous operation profiling. The sink runs after
// collection stops. Durations are wall time, not CPU/GPU time; self time is
// inclusive minus instrumented children, including uninstrumented work.
class HKSR_API OperationTrace {
public:
    using Sink=std::function<void(const OperationTraceReport&)>;
    OperationTrace(const char* stage,std::uint64_t subject,std::uint64_t generation,
        std::uint64_t frame,const char* source,const Sink& sink) noexcept;
    ~OperationTrace();
    OperationTrace(const OperationTrace&)=delete;
    OperationTrace& operator=(const OperationTrace&)=delete;
private:
    void* state_=nullptr;
};
class HKSR_API OperationSpan {
public:
    explicit OperationSpan(const char* stage,std::uintptr_t native_target=0) noexcept;
    ~OperationSpan();
    void Failed() noexcept {failed_=true;}
    OperationSpan(const OperationSpan&)=delete;
    OperationSpan& operator=(const OperationSpan&)=delete;
private:
    void* state_=nullptr;
    OperationSpan* previous_=nullptr;
    std::uint32_t row_=0;
    std::uint64_t start_=0,children_=0;
    bool failed_=false;
};
}
