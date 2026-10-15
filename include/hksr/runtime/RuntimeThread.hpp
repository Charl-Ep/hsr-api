#pragma once
#include "../managed/References.hpp"
#include "GameVersion.hpp"
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
// Native API-table functions: NO MethodInfo parameter. These are not Method.
struct RuntimeThreadMethods {
    void* domain=nullptr;
    void* current=nullptr;
    void* attach=nullptr;
    void* detach=nullptr;
    std::vector<std::string> binding_errors;
};
struct RuntimeThreadStatus {
    unsigned long owner_thread=0;
    ObjectResult before, domain, attached;
    CallResult detached;
    bool started=false, ready=false, owned=false, detach_attempted=false, closed=false;
    std::string error;
};
// Metadata/bootstrap worker attachment only; being attached does NOT make a
// worker the Unity main thread. No worker creation, gameplay calls or implicit
// destructor detach. Enter/Leave must run on the same OS thread, outside DllMain.
class HKSR_API RuntimeThread {
public:
    explicit RuntimeThread(RuntimeThreadMethods methods);
    ~RuntimeThread(); // native storage only; does not detach
    RuntimeThread(const RuntimeThread&)=delete;
    RuntimeThread& operator=(const RuntimeThread&)=delete;
    const RuntimeThreadStatus& Enter();
    const RuntimeThreadStatus& Leave();
    const RuntimeThreadStatus& Status() const;
private:
    struct State;
    State* state_;
};
// Resolves the selected qualified table; does not grant main-thread ownership.
HKSR_API RuntimeThreadMethods ResolveRuntimeThreadMethods(RuntimeVersion version);
}
