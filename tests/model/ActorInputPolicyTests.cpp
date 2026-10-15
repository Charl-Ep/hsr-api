#include <hksr/native/ActorInputPolicy.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/model/ModelVisibilityLease.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <thread>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool value) {if(!value)throw std::runtime_error("actor input policy assertion");}
int info,owned,foreign,input,replacement,string_type,empty_array;
void* observed=nullptr;
int calls=0,allocations=0,next_root=0;
std::map<int,void*> roots;
__declspec(noinline) void __fastcall Outfit(void* receiver,void* argument,void* metadata) {
    Check(receiver && metadata==&info);++calls;observed=argument;
}
__declspec(noinline) void __fastcall Weapons(void* receiver,void* argument,void* metadata) {
    Check(receiver && metadata==&info);calls+=2;observed=argument;
}
template<class T> Method M(T fn) {return {reinterpret_cast<void*>(fn),&info};}
void Invoke(Method method,void* receiver,void* argument) {
    Check(native::CallVoid(method,receiver,argument).returned);
    Check(observed==argument);
}
int __fastcall Keep(void* object,int,int,void*) {roots[++next_root]=object;return next_root;}
void* __fastcall Target(int id,void*) {return roots.at(id);}
void __fastcall Free(int id,void*) {roots.erase(id);}
void* __fastcall Array(void* type,int count,void*) {
    Check(type==&string_type && count==0);++allocations;return &empty_array;
}
struct Objects final:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* type) override {
        Check(std::string(ns)=="System" && std::string(type)=="String");return {{true,0},&string_type};
    }
    ObjectResult NewStringUtf8(const char*) override {throw std::runtime_error("unexpected allocation");}
    ObjectResult AllocateObject(const char*,const char*) override {throw std::runtime_error("unexpected allocation");}
};
struct Methods final:MethodResolver {
    Method Resolve(const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> params,MethodKind kind) override {
        Check(std::string(ns)=="RPG.GameCore" && std::string(type)=="CharacterModelComponent" &&
            std::string(name)=="_HideWeapons" && std::string(result)=="System.Void" &&
            params.size()==1 && std::string(*params.begin())=="System.String[]" && kind==MethodKind::Instance);
        return M(Weapons);
    }
};
}
int main() try {
    // A shared hook never suppresses the original call or substitutes an
    // input belonging to another owner/thread/client.
    ActorInputPolicy outfit(ActorInputKind::OutfitIdentity,M(Outfit),[](void* self,void* value)->std::optional<void*> {
        if(self==&owned && !value)return &replacement;
        return {};
    });
    outfit.ActivateOnCurrentThread();
    Invoke(M(Outfit),&foreign,nullptr);Invoke(M(Outfit),&owned,&input);
    Check(native::CallVoid(M(Outfit),&owned,static_cast<void*>(nullptr)).returned && observed==&replacement);
    std::thread worker([]{Invoke(M(Outfit),&owned,nullptr);});worker.join();
    {
        ActorInputPolicy second(ActorInputKind::OutfitIdentity,M(Outfit),[](void* self,void* value)->std::optional<void*> {
            if(self==&foreign && !value)return &input;
            return {};
        });
        second.ActivateOnCurrentThread();
        Check(native::CallVoid(M(Outfit),&foreign,static_cast<void*>(nullptr)).returned && observed==&input);
        outfit.Stop();Invoke(M(Outfit),&owned,nullptr);
        Check(native::CallVoid(M(Outfit),&foreign,static_cast<void*>(nullptr)).returned && observed==&input);
    }
    Invoke(M(Outfit),&foreign,nullptr);Check(calls==8);

    Objects objects;Methods methods;
    const auto owner=TrackedEntities().Track(reinterpret_cast<GameEntity*>(&owned));
    ModelVisibilityLease lease(methods,objects,{M(Keep),M(Target),M(Free),{},{}},ManagedArrays({M(Array),{},{},{},{}}));
    Check(roots.empty());
    Check(lease.Select(owner,&owned,false));Invoke(M(Weapons),&owned,&input);Check(allocations==0);
    Check(lease.Select(owner,&owned,true));Check(roots.size()==2 && allocations==1);
    Check(native::CallVoid(M(Weapons),&owned,&input).returned && observed==&empty_array);
    Invoke(M(Weapons),&foreign,&input);
    std::thread foreign_thread([]{Invoke(M(Weapons),&owned,&input);});foreign_thread.join();
    Check(lease.Select(owner,&owned,false));Invoke(M(Weapons),&owned,&input);
    Check(lease.Select(owner,&owned,true) && allocations==1);
    TrackedEntities().Disposed(owner);TrackedEntities().Forget(owner);
    const auto newer=TrackedEntities().Track(reinterpret_cast<GameEntity*>(&owned));
    Check(newer.generation!=owner.generation);Invoke(M(Weapons),&owned,&input);
    Check(lease.Close() && roots.empty() && lease.Close());Invoke(M(Weapons),&owned,&input);
    TrackedEntities().Disposed(newer);TrackedEntities().Forget(newer);
    std::cout<<"Actor input isolation and model-domain visibility passed.\n";return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
