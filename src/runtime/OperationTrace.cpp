#include <hksr/runtime/OperationTrace.hpp>
#include <Windows.h>
#include <chrono>
#include <algorithm>
#include <map>
#include <tuple>

namespace hksr::sdk_v1 {
namespace {
std::uint64_t Now() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct TraceState {
    OperationTraceReport report;
    OperationTrace::Sink sink;
    TraceState* previous=nullptr;
    OperationSpan* span=nullptr;
    std::uint32_t current=0;
    std::uint64_t start=0,children=0;
    std::map<std::tuple<std::uint32_t,std::string,std::uintptr_t>,std::uint32_t> indices;
};
thread_local TraceState* trace=nullptr;
void Add(OperationTiming& row,std::uint64_t elapsed,std::uint64_t children,bool failed=false) noexcept {
    ++row.calls;row.inclusive_ns+=elapsed;
    row.self_ns+=elapsed>=children?elapsed-children:0;
    row.max_ns=std::max(row.max_ns,elapsed);row.failures+=failed?1:0;
}
void DescribeTarget(OperationTiming& row) {
    if(!row.native_target)return;
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(row.native_target),&module))return;
    wchar_t path[32768]{};
    const auto size=GetModuleFileNameW(module,path,32768);if(!size || size>=32768)return;
    const auto* name=wcsrchr(path,L'\\');name=name?name+1:path;
    const auto bytes=WideCharToMultiByte(CP_UTF8,0,name,-1,nullptr,0,nullptr,nullptr);
    if(bytes<=0)return;
    row.native_module.resize(static_cast<std::size_t>(bytes));
    WideCharToMultiByte(CP_UTF8,0,name,-1,row.native_module.data(),bytes,nullptr,nullptr);
    row.native_module.pop_back();
    row.native_rva=row.native_target-reinterpret_cast<std::uintptr_t>(module);
}
}
OperationTrace::OperationTrace(const char* stage,std::uint64_t subject,std::uint64_t generation,
    std::uint64_t frame,const char* source,const Sink& sink) noexcept {
    if(!sink)return;
    const auto error=GetLastError();
    TraceState* s=nullptr;
    try {
        s=new TraceState;s->report.subject=subject;s->report.generation=generation;
        s->report.frame=frame;s->report.tick_ms=GetTickCount64();s->report.source=source?source:"";
        s->report.rows.push_back({0,stage?stage:"operation"});
        s->sink=sink;s->previous=trace;s->start=Now();trace=s;state_=s;
    }catch(...){delete s;}
    SetLastError(error);
}
OperationTrace::~OperationTrace() {
    if(!state_)return;
    const auto error=GetLastError();auto* s=static_cast<TraceState*>(state_);
    Add(s->report.rows.front(),Now()-s->start,s->children);trace=s->previous;
    try {for(auto& row:s->report.rows)DescribeTarget(row);}catch(...){++s->report.dropped;}
    try{s->sink(s->report);}catch(...){} // Reporting never changes operation outcome.
    delete s;SetLastError(error);
}
OperationSpan::OperationSpan(const char* stage,std::uintptr_t target) noexcept {
    if(!trace)return;
    const auto error=GetLastError();auto* s=trace;
    try {
        const auto key=std::make_tuple(s->current,std::string(stage?stage:"stage"),target);
        auto it=s->indices.find(key);
        if(it==s->indices.end()) {
            const auto index=static_cast<std::uint32_t>(s->report.rows.size());
            s->report.rows.push_back({s->current,std::get<1>(key),target});
            it=s->indices.emplace(key,index).first;
        }
        row_=it->second;previous_=s->span;s->span=this;s->current=row_;state_=s;start_=Now();
    }catch(...){++s->report.dropped;}
    SetLastError(error);
}
OperationSpan::~OperationSpan() {
    if(!state_)return;
    const auto error=GetLastError();auto* s=static_cast<TraceState*>(state_);
    const auto elapsed=Now()-start_;auto& row=s->report.rows[row_];
    Add(row,elapsed,children_,failed_);s->current=row.parent;s->span=previous_;
    if(previous_)previous_->children_+=elapsed;else s->children+=elapsed;
    SetLastError(error);
}
}
