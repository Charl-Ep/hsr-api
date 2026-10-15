#include <Windows.h>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/control/ActorControl.hpp>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int metadata,mesh,entity_storage,head_a,head_b;
std::map<int,void*> roots;
int next_root=0,retains=0,frees=0;
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&metadata};}
int __fastcall Keep(void* object,int,int,void*){++retains;roots[++next_root]=object;return next_root;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1,"duplicate release");++frees;}
bool __fastcall True(void*,void*){return true;}
void* __fastcall Identity(void* object,void*){return object;}
int __fastcall Vertices(void*,void*){return 42;}
int __fastcall Length(void*,void*){return 3;}
void* __fastcall Animator(void*,void*){return &mesh;}
void* __fastcall HeadA(void*,void*){return &head_a;}
void* __fastcall HeadB(void*,void*){return &head_b;}
struct Objects final:ManagedObjects {
    ObjectResult ManagedType(const char*,const char*) override {throw std::runtime_error("unexpected type query");}
    ObjectResult NewStringUtf8(const char*) override {throw std::runtime_error("unexpected string allocation");}
    ObjectResult AllocateObject(const char*,const char*) override {throw std::runtime_error("unexpected allocation");}
};
struct Metadata final:ObjectMetadata {
    DeclaredType ReadObjectType(void*) override {return {"Test","Model",{{"get_Head","UnityEngine.Transform",{},false,M(HeadB)}}};}
    std::vector<DeclaredField> ReadObjectFields(void*) override {return {};}
};
void CheckCopyIsolation() {
    ActorControlMethods methods;methods.head=M(HeadA);
    ActorControl original(methods);auto changed=original;auto assigned=original;
    Metadata declarations;changed.BindMono(declarations,&entity_storage);
    Check(original.Head(&entity_storage).object==&head_a,"rebind mutated original handle");
    Check(changed.Head(&entity_storage).object==&head_b,"rebind failed");
    assigned=changed;
    Check(assigned.Head(&entity_storage).object==&head_b,"assignment lost bindings");
    auto moved=std::move(assigned);
    Check(moved.Head(&entity_storage).object==&head_b && assigned.Head(&entity_storage).object==&head_b,
        "immutable handle move lost callable state");
}
}
int wmain(int argc,wchar_t** argv) try {
    Check(argc==2,"expected current or frozen consumer DLL path");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    auto module=LoadLibraryW(argv[1]);Check(module!=nullptr,"consumer load failed");
    using Open=void*(*)(const PresentationContext*) noexcept;
    using Exercise=bool(*)(void*,EntityToken,void*,CharacterModelComponent*) noexcept;
    using Close=void(*)(void*) noexcept;
    const auto open=reinterpret_cast<Open>(GetProcAddress(module,"ConsumerOpen"));
    const auto exercise=reinterpret_cast<Exercise>(GetProcAddress(module,"ConsumerExercise"));
    const auto close=reinterpret_cast<Close>(GetProcAddress(module,"ConsumerClose"));
    Check(open && exercise && close,"consumer capability exports missing");
    CheckCopyIsolation();
    auto* entity=reinterpret_cast<GameEntity*>(&entity_storage);
    const auto token=TrackedEntities().Track(entity);
    Objects objects;
    // Two provider-side field layouts, with the same consumer binary. The
    // consumer sees only snapshots and never calculates a managed field offset.
    for(std::size_t slot=0;slot<2;++slot) {
        void* fields[2]{};fields[slot]=&mesh;
        PresentationBindings b;b.references={M(Keep),M(Target),M(Free),{}, {}};
        b.entity.loaded=M(True);b.mesh.vertex_count=M(Vertices);b.arrays.length=M(Length);
        b.model.has_root_field=true;b.model.root_offset=slot*sizeof(void*);b.model.animator=M(Animator);
        b.animation.controller=M(Identity);b.profile_inputs.animator_initialized=M(True);
        void* handle=nullptr;
        {
            PresentationContext context(objects,std::move(b));handle=open(&context);
            Check(handle!=nullptr,"consumer capability acquisition failed");
            Check(context.Close(),"empty context failed to close");
        }
        const bool ok=exercise(handle,token,&mesh,reinterpret_cast<CharacterModelComponent*>(fields));
        close(handle);
        Check(ok,"consumer capability round trip failed");
        Check(roots.empty(),"consumer leaked roots");
    }
    Check(retains==6 && frees==retains,"independent resource ownership not exercised");
    TrackedEntities().Disposed(token);TrackedEntities().Forget(token);
    FreeLibrary(module);
    std::cout<<"Consumer copies, model/resource reads, retained factories and independent session cleanup passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
