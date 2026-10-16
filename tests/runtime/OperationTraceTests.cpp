#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <Windows.h>
#include <thread>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr::api;
void Check(bool value){if(!value)throw std::runtime_error("operation trace assertion");}
int object,info;
int __fastcall Length(void* self,void* metadata) {
    Check(self==&object && metadata==&info);
    OperationSpan nested("official.callback");
    return 7;
}
int __fastcall Failure(void*,void*) {RaiseException(0xE0123489,0,0,nullptr);return 0;}
template<class T> Method M(T f){return {reinterpret_cast<void*>(f),&info};}
}
int main() try {
    ManagedArrays arrays({{},M(Length),{},{},{}});
    ManagedArrays bad({{},M(Failure),{},{},{}});
    std::vector<OperationTraceReport> reports;
    {
        // Temporary sink lifetime is owned by the trace, not the expression.
        OperationTrace trace("create",4,2,9,"fixture",[&](const auto& r){reports.push_back(r);});
        SetLastError(1234);
        {
            OperationSpan parent("model");Check(GetLastError()==1234);
            for(int i=0;i<3;++i)Check(arrays.Length(&object).value==7);
            const auto result=bad.Length(&object);Check(!result.read.returned && result.read.exception==0xE0123489);
            std::thread foreign([&]{Check(arrays.Length(&object).value==7);});foreign.join();
        }
        try {OperationSpan throwing("throwing");throw std::runtime_error("fixture");}catch(const std::runtime_error&){}
        {
            OperationTrace inner("nested-root",5,1,9,"other",[&](const auto& r){reports.push_back(r);});
            OperationSpan isolated("isolated");
        }
        {OperationSpan restored("restored");}
    }
    Check(reports.size()==2 && reports[0].subject==5 && reports[1].subject==4);
    const auto& r=reports[1];Check(r.generation==2 && r.frame==9 && r.source=="fixture" && !r.dropped);
    bool length=false,failure=false,restored=false;
    for(std::size_t i=0;i<r.rows.size();++i) {
        const auto& row=r.rows[i];Check(row.self_ns<=row.inclusive_ns && row.max_ns<=row.inclusive_ns);
        if(i)Check(row.parent<i);
        if(row.native_target==reinterpret_cast<std::uintptr_t>(Length)){Check(row.calls==3 && !row.failures);length=true;}
        if(row.native_target==reinterpret_cast<std::uintptr_t>(Failure)){Check(row.calls==1 && row.failures==1);failure=true;}
        if(row.stage=="restored")restored=true;
        Check(row.stage!="isolated");
        std::uint64_t children=0;
        for(std::size_t j=1;j<r.rows.size();++j)if(r.rows[j].parent==i)children+=r.rows[j].inclusive_ns;
        Check(row.self_ns+children==row.inclusive_ns);
    }
    Check(length && failure && restored);
    {OperationTrace disabled("disabled",0,0,0,"",{});Check(arrays.Length(&object).value==7);}
    {OperationTrace sink_failure("sink",0,0,0,"",[](const auto&){throw std::runtime_error("sink");});}
    Check(arrays.Length(&object).value==7);
    std::cout<<"Operation trace: nesting, aggregation, native receipt, thread isolation, unwinding and optional sink passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
