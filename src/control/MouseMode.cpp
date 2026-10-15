#include <hksr/control/MouseMode.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../platform/NativeHooks.hpp"
#include <mutex>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
thread_local bool own_request=false;
struct OwnScope {bool prior=own_request;OwnScope(){own_request=true;}~OwnScope(){own_request=prior;}};
struct OwnerObservation {std::uint64_t serial=0;bool forced=false;};
struct Observer {
    detail::NativeHooks hooks;std::mutex mutex;
    std::map<void*,OwnerObservation> requests;std::size_t offset=0;
    static Observer& Get(){static auto* value=new Observer;return *value;}
    void Record(void* object) noexcept {
        if(own_request || !object)return;
        bool forced=false;if(!native::CopyMemoryChecked(&forced,static_cast<const char*>(object)+offset,sizeof(forced)))return;
        try {std::lock_guard<std::mutex> guard(mutex);auto& row=requests[object];++row.serial;row.forced=forced;}catch(...){}
    }
    OwnerObservation Read(void* object) {std::lock_guard<std::mutex> guard(mutex);return requests[object];}
    template<int Index> static void __fastcall Call(void* object,void* info) {
        auto& s=Get();const bool own=own_request;own_request=false;
        reinterpret_cast<void(__fastcall*)(void*,void*)>(s.hooks.Original(Index))(object,info);
        if(!own)s.Record(object);own_request=own;
    }
};
void Returned(CallResult result) {if(!result.returned)throw std::runtime_error("official mouse mode call failed");}
void OwnCall(Method method,void* object) {OwnScope own;Returned(native::CallVoid(method,object));}
}
MouseModeMethods ResolveMouseMode(RuntimeMetadata& m) {
    MouseModeMethods out;
    out.instance=m.Resolve("RPG.Client","MouseInputController","get_Instance","RPG.Client.MouseInputController",{},MethodKind::Static);
    out.force=m.Resolve("RPG.Client","MouseInputController","ForceShowCursor","System.Void",{});
    out.force_sdk=m.Resolve("RPG.Client","MouseInputController","ForceShowCursorBySDKUI","System.Void",{});
    out.close=m.Resolve("RPG.Client","MouseInputController","CloseForceShowCursor","System.Void",{});
    out.hide=m.Resolve("RPG.Client","MouseInputController","HideCursor","System.Void",{});
    out.refresh=m.Resolve("RPG.Client","MouseInputController","_ShowCursor","System.Void",{});
    out.use_mode=m.Resolve("RPG.Client","MouseInputController","get_UseCursorMode","System.Boolean",{});
    out.visible=m.Resolve("UnityEngine","Cursor","get_visible","System.Boolean",{},MethodKind::Static);
    out.lock=m.Resolve("UnityEngine","Cursor","get_lockState","UnityEngine.CursorLockMode",{},MethodKind::Static);
    for(const auto& f:m.ReadFields("RPG.Client","MouseInputController",false))if(f.name=="_ForceShowCursor" && f.type=="System.Boolean" && f.flags_observed && !(f.flags&0x10))out.forced_offset=f.offset;
    if(!out.forced_offset)throw std::runtime_error("official cursor ownership field unavailable");
    return out;
}
void ObserveMouseModeOwners(const MouseModeMethods& m) {
    auto& s=Observer::Get();s.offset=m.forced_offset;
    s.hooks.Install({{"Mouse.Force",m.force.code,reinterpret_cast<void*>(Observer::Call<0>)},
        {"Mouse.SDKForce",m.force_sdk.code,reinterpret_cast<void*>(Observer::Call<1>)},
        {"Mouse.Close",m.close.code,reinterpret_cast<void*>(Observer::Call<2>)}});
}
struct MouseModeLease::State {
    MouseModeMethods methods;References references;
    void* object=nullptr;int root=0;bool external=false;std::uint64_t serial=0;
    std::string status;
    State(MouseModeMethods m,ReferenceMethods r):methods(m),references(r){}
    bool Forced() const {
        bool value=false;
        if(!native::CopyMemoryChecked(&value,static_cast<const char*>(object)+methods.forced_offset,sizeof(value)))throw std::runtime_error("official cursor ownership read failed");
        return value;
    }
};
MouseModeLease::MouseModeLease(MouseModeMethods m,ReferenceMethods r):state_(new State(m,r)){}
MouseModeLease::~MouseModeLease(){delete state_;} // Service(false) on official frame; no destructor engine calls
std::string MouseModeLease::Status() const{return state_->status;}
void MouseModeLease::Service(bool requested) {
    auto& s=*state_;
    try {
        if(!s.object) {
            if(!requested)return;
            void* object=nullptr;Returned(native::Call(s.methods.instance,&object));
            if(!object)throw std::runtime_error("official mouse controller unavailable");
            const auto kept=s.references.Retain(object);
            if(!kept.call.returned || !kept.handle)throw std::runtime_error("mouse controller retention failed");
            s.object=object;s.root=kept.handle;s.external=s.Forced();s.serial=Observer::Get().Read(s.object).serial;
        }
        const auto current=Observer::Get().Read(s.object);
        if(current.serial!=s.serial){s.serial=current.serial;s.external=current.forced;}
        if(requested) {
            bool mode=false,visible=false;int lock=0;
            Returned(native::Call(s.methods.use_mode,&mode,s.object));
            Returned(native::Call(s.methods.visible,&visible));Returned(native::Call(s.methods.lock,&lock));
            // AdventureInputProvider clears the force flag during its normal
            // update without leaving cursor mode. Reacquiring just for that
            // flag rebroadcasts the official cursor notification every frame.
            // Healthy mode is already the requested outcome; keep its inputs.
            if((!mode || !visible || lock!=0) && !s.Forced()) {
                OwnCall(s.methods.force,s.object);
                Returned(native::Call(s.methods.use_mode,&mode,s.object));
                Returned(native::Call(s.methods.visible,&visible));Returned(native::Call(s.methods.lock,&lock));
            }
            // Focus recovery can invalidate native cursor state independently.
            if(s.Forced() && mode && (!visible || lock!=0))Returned(native::CallVoid(s.methods.refresh,s.object));
            s.status=mode?"官方鼠标模式已接管":"官方鼠标模式当前未获准";
        } else {
            if(!s.external) {
                OwnCall(s.methods.close,s.object);
                Returned(native::CallVoid(s.methods.hide,s.object)); // retains official Alt/UI/platform checks
            }
            const auto root=s.root;s.root=0;s.object=nullptr;Returned(s.references.Free(root));s.status="鼠标已交还官方策略";
        }
    }catch(const std::exception& e){s.status=e.what();}
}
}
