#include <hksr/animation/AnimatorCatalog.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../adapters/animation/AnimatorLayout.hpp"
#include "../../adapters/animation/AnimatorTable.hpp"
#include <array>
#include <algorithm>

namespace hksr::sdk_v1 {
namespace {
using namespace detail;
const TableRead memory=[](std::uintptr_t address,void* output,std::size_t size) {
    return native::CopyMemoryChecked(output,reinterpret_cast<void*>(address),size);
};
void Require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}

}
struct AnimatorCatalog::State {
    Method cached,layers,layer_name,resolve,hash,has,initialized;
    AnimatorLayout layout;
    ManagedRoots roots;
    ComponentConfig text;
    std::vector<ManagedRootId> pending;
    explicit State(RuntimeMetadata& metadata):roots(ResolveReferenceMethods(metadata)),text(ResolveComponentConfigMethods(metadata)) {
        auto animator=[&](const char* name,const char* result,std::initializer_list<const char*> args={}){return metadata.Resolve("UnityEngine","Animator",name,result,args);};
        cached=metadata.Resolve("UnityEngine","Object","GetCachedPtr","System.IntPtr",{});
        layers=animator("get_layerCount","System.Int32");layer_name=animator("GetLayerName","System.String",{"System.Int32"});
        resolve=animator("ResolveHash","System.String",{"System.Int32"});has=animator("HasState","System.Boolean",{"System.Int32","System.Int32"});
        initialized=animator("get_isInitialized","System.Boolean");
        hash=metadata.Resolve("UnityEngine","Animator","StringToHash","System.Int32",{"System.String"},MethodKind::Static);
        layout=LocateAnimatorLayout(animator("GetAnimatorStateInfo","System.Void",{"System.Int32","UnityEngine.StateInfoIndex","UnityEngine.AnimatorStateInfo&"}),metadata.Version());
    }
    std::string Name(Method method,void* animator,int id,bool* roundtrip=nullptr) {
        void* value=nullptr;Require(native::Call(method,&value,animator,id).returned,"official name query failed");
        if(!value)return {};
        const auto root=roots.Keep(value);pending.push_back(root);Require(roots.Status(root).ready,"name root failed");
        auto result=text.ReadText(value);Require(result.read.returned,"name string read failed");
        if(roundtrip){int actual=0;*roundtrip=native::Call(hash,&actual,value).returned && actual==id;}
        if(roots.Release(root).complete){roots.Forget(root);pending.pop_back();}
        return result.utf8;
    }
};
AnimatorCatalog::AnimatorCatalog(RuntimeMetadata& metadata):state_(new State(metadata)){}
AnimatorCatalog::~AnimatorCatalog(){delete state_;}
ControllerCatalog AnimatorCatalog::Read(void* animator) {
    auto& s=*state_;ControllerCatalog result;
    try {
        bool initialized=false;Require(animator && native::Call(s.initialized,&initialized,animator).returned && initialized,"Animator is not initialized");
        int layers=0;Require(native::Call(s.layers,&layers,animator).returned,"official layer count unavailable");
        void* native=nullptr;Require(native::Call(s.cached,&native,animator).returned && native,"native Animator unavailable");
        const auto object=reinterpret_cast<std::uintptr_t>(native);
        Require(TableValue<std::uintptr_t>(memory,object+s.layout.controller)!=0 && TableValue<std::uintptr_t>(memory,object+s.layout.playable_count)!=0,"Animator has no controller playable");
        const auto playable=TableValue<std::uintptr_t>(memory,TableValue<std::uintptr_t>(memory,object+s.layout.playable));
        const auto root=TableValue<std::uintptr_t>(memory,playable+s.layout.constant);
        result=ReadAnimatorTable(memory,root,layers);
        for(auto& layer:result.layers)try{layer.name=s.Name(s.layer_name,animator,layer.index);}catch(const std::exception& e){result.issues.push_back("layer name: "+std::string(e.what()));}
        for(auto& row:result.states) {
            try {
                row.name=s.Name(s.resolve,animator,row.short_hash);
                row.path=s.Name(s.resolve,animator,row.path_hash);
                row.full_path=s.Name(s.resolve,animator,row.full_path_hash,&row.name_roundtrip);
                const auto accepted=native::Call(s.has,&row.recognized,animator,row.layer,row.full_path_hash);
                if(!accepted.returned || !row.recognized || !row.name_roundtrip || row.full_path.empty())
                    result.issues.push_back("state identity not confirmed: layer="+std::to_string(row.layer)+" index="+std::to_string(row.index));
            }catch(const std::exception& e){result.issues.push_back("state "+std::to_string(row.layer)+":"+std::to_string(row.index)+" "+e.what());}
        }
    }catch(const std::exception& e){result.issues.push_back(e.what());}
    for(auto it=s.pending.begin();it!=s.pending.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.pending.erase(it);}else ++it;
    }
    result.complete=result.issues.empty();return result;
}
}
