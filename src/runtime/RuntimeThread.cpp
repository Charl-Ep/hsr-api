#include <hksr/runtime/RuntimeThread.hpp>
#include <Windows.h>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct RuntimeThread::State { RuntimeThreadMethods methods; RuntimeThreadStatus status; };
RuntimeThread::RuntimeThread(RuntimeThreadMethods methods):state_(new State{std::move(methods),{}}){}
RuntimeThread::~RuntimeThread(){delete state_;}
const RuntimeThreadStatus& RuntimeThread::Status() const{return state_->status;}
namespace {
ObjectResult NoArgs(void* function) {
    ObjectResult result;
    if(!function)return result;
    __try {result.object=reinterpret_cast<void*(__fastcall*)()>(function)();result.call.returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){result.call.exception=GetExceptionCode();}
    return result;
}
ObjectResult Attach(void* function,void* domain) {
    ObjectResult result;
    __try {result.object=reinterpret_cast<void*(__fastcall*)(void*)>(function)(domain);result.call.returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){result.call.exception=GetExceptionCode();}
    return result;
}
CallResult Detach(void* function,void* thread) {
    CallResult result;
    __try {reinterpret_cast<void(__fastcall*)(void*)>(function)(thread);result.returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){result.exception=GetExceptionCode();}
    return result;
}
}
const RuntimeThreadStatus& RuntimeThread::Enter(){
    auto& status_=state_->status;
    const auto& methods_=state_->methods;
    if(status_.started && status_.owner_thread!=GetCurrentThreadId())
        throw std::logic_error("runtime thread attachment belongs to another OS thread");
    if(status_.started || status_.closed)return status_;
    status_.started=true;status_.owner_thread=GetCurrentThreadId();
    status_.before=NoArgs(methods_.current);
    if(!status_.before.call.returned){status_.error="current engine thread query failed";return status_;}
    if(status_.before.object){status_.ready=true;return status_;} // borrowed
    if(!methods_.attach || !methods_.detach){status_.error="owned attachment requires its paired detach";return status_;}
    status_.domain=NoArgs(methods_.domain);
    if(!status_.domain.call.returned || !status_.domain.object){status_.error="engine domain unavailable";return status_;}
    status_.attached=Attach(methods_.attach,status_.domain.object);
    if(!status_.attached.call.returned || !status_.attached.object){status_.error="engine thread attach failed";return status_;}
    status_.owned=true;status_.ready=true;
    return status_;
}
const RuntimeThreadStatus& RuntimeThread::Leave(){
    auto& status_=state_->status;
    const auto& methods_=state_->methods;
    if(status_.closed || status_.detach_attempted)return status_;
    if(status_.started && status_.owner_thread!=GetCurrentThreadId()){
        status_.error="engine thread detach requested on another OS thread";return status_;
    }
    status_.error.clear();
    status_.ready=false;
    if(status_.owned){
        status_.detach_attempted=true;
        status_.detached=Detach(methods_.detach,status_.attached.object);
        status_.closed=status_.detached.returned;
        if(!status_.closed)status_.error="engine thread detach outcome unknown";
    }else{
        // A faulting attach may have changed runtime TLS without returning its
        // token. Do not invent a pointer or report clean worker detachment.
        status_.closed=!status_.attached.call.exception;
        if(!status_.closed)status_.error="engine thread attach outcome unknown";
    }
    return status_;
}
}
