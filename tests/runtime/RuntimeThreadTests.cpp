#include <hksr/runtime/RuntimeThread.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace hksr::api;
int domain,token,attaches=0,detaches=0,domains=0;
bool already=false,attach_fault=false,detach_fault=false,null_domain=false;
void Check(bool value){if(!value)throw std::runtime_error("runtime thread test failed");}
void* __fastcall Domain(){++domains;return null_domain?nullptr:&domain;}
void* __fastcall Current(){return already?&token:nullptr;}
void* __fastcall Attach(void* input){Check(input==&domain);++attaches;if(attach_fault)RaiseException(0xE0630020,0,0,nullptr);return &token;}
void __fastcall Detach(void* input){Check(input==&token);++detaches;if(detach_fault)RaiseException(0xE0630021,0,0,nullptr);}
RuntimeThreadMethods Methods(){return {reinterpret_cast<void*>(&Domain),reinterpret_cast<void*>(&Current),
    reinterpret_cast<void*>(&Attach),reinterpret_cast<void*>(&Detach),{}};}
}
void TestRuntimeThread(){
    RuntimeThread owned(Methods());Check(owned.Enter().ready && owned.Status().owned && attaches==1);
    Check(owned.Enter().ready && attaches==1);
    bool wrong_enter=false;
    std::thread other([&]{try{owned.Enter();}catch(const std::logic_error&){wrong_enter=true;}owned.Leave();});other.join();
    Check(wrong_enter);
    Check(owned.Status().ready && !owned.Status().detach_attempted && detaches==0);
    Check(owned.Leave().closed && detaches==1);owned.Leave();Check(detaches==1);
    already=true;RuntimeThread borrowed(Methods());const auto prior=domains;
    Check(borrowed.Enter().ready && !borrowed.Status().owned && domains==prior);
    Check(borrowed.Leave().closed && detaches==1);already=false;
    auto missing=Methods();missing.detach=nullptr;RuntimeThread no_pair(missing);
    Check(!no_pair.Enter().ready && attaches==1 && no_pair.Leave().closed);
    null_domain=true;RuntimeThread waiting(Methods());Check(!waiting.Enter().ready && waiting.Leave().closed);null_domain=false;
    attach_fault=true;RuntimeThread unknown_attach(Methods());Check(!unknown_attach.Enter().ready);
    Check(!unknown_attach.Leave().closed && !unknown_attach.Status().detach_attempted);attach_fault=false;
    RuntimeThread unknown_detach(Methods());Check(unknown_detach.Enter().ready);detach_fault=true;
    Check(!unknown_detach.Leave().closed && unknown_detach.Status().detached.exception==0xE0630021);
    const auto attempts=detaches;detach_fault=false;unknown_detach.Leave();Check(detaches==attempts);
    already=true;RuntimeThread only_current({nullptr,reinterpret_cast<void*>(&Current),nullptr,nullptr,{}});
    Check(only_current.Enter().ready && only_current.Leave().closed);already=false;
    std::cout<<"Runtime thread attachment passed: native ABI, borrowed/owned distinction, same-thread paired exit, unknown outcomes not retried.\n";
}
