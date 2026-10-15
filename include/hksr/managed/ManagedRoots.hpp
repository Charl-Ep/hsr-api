#pragma once
#include "References.hpp"

namespace hksr::sdk_v1 {
using ManagedRootId = std::uint64_t;
struct ManagedRootStatus {
    ManagedRootId id = 0;
    RootResult retain;
    ObjectResult target;
    CallResult free;
    bool release_attempted = false;
    bool ready = false;
    bool complete = false;
};
// Owns only GC handles, NOT a CachedAssetLoader lease or Unity native object.
// Engine-thread operations. Explicit Release; destructor calls no engine code.
// Keep alive until handles are closed or the engine has ended.
class HKSR_API ManagedRoots {
public:
    explicit ManagedRoots(ReferenceMethods methods);
    ~ManagedRoots();
    ManagedRoots(const ManagedRoots&) = delete;
    ManagedRoots& operator=(const ManagedRoots&) = delete;
    ManagedRootId Keep(void* object);
    ObjectResult Target(ManagedRootId id);
    ManagedRootStatus Status(ManagedRootId id) const;
    ManagedRootStatus Release(ManagedRootId id);
    void Forget(ManagedRootId id);
private:
    friend class ManagedWeakReferences;
    ManagedRoots(ReferenceMethods methods,bool weak);
    struct State;
    State* state_;
};

// Non-owning identity observations. A successful Target may return null after
// collection; errors must NOT be interpreted as collection. Ordinary weak
// handles do not retain the object or track it through resurrection.
// The caller must root a non-null target before invoking managed operations.
// Explicit Release/Forget, with the same no-retry receipts as ManagedRoots.
class ManagedWeakReferences {
public:
    explicit ManagedWeakReferences(ReferenceMethods methods):handles_(methods,true){}
    ManagedRootId Watch(void* object){return handles_.Keep(object);}
    ObjectResult Target(ManagedRootId id){return handles_.Target(id);}
    ManagedRootStatus Status(ManagedRootId id)const{return handles_.Status(id);}
    ManagedRootStatus Release(ManagedRootId id){return handles_.Release(id);}
    void Forget(ManagedRootId id){handles_.Forget(id);}
private:
    ManagedRoots handles_;
};
}
