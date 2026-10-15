#pragma once
#include "References.hpp"

namespace hksr::sdk_v1 {
// The runtime metadata provider implements allocation from the CURRENT domain.
// This boundary also permits consumer tests without opening a game process.
// Results are unrooted, AllocateObject does not call a constructor.
struct HKSR_API ManagedObjects {
    virtual ~ManagedObjects()=default;
    virtual ObjectResult NewStringUtf8(const char* text)=0;
    virtual ObjectResult AllocateObject(const char* name_space,const char* name)=0;
    virtual ObjectResult ManagedType(const char* name_space,const char* name)=0;
};
}
