#include <hksr/presentation/audio/SoundCallbacks.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
using Sink=std::function<void(SoundCallbackEvent)>;
struct Dispatch {
    std::mutex mutex;
    std::uintptr_t next=0;
    std::map<std::uintptr_t,Sink> sinks;
    static Dispatch& Get(){static auto* value=new Dispatch;return *value;}
    std::uintptr_t Add(Sink sink) {
        std::lock_guard<std::mutex> lock(mutex);
        if(next==std::numeric_limits<std::uintptr_t>::max())throw std::overflow_error("sound callback identity exhausted");
        const auto id=++next;sinks.emplace(id,std::move(sink));return id;
    }
    void Remove(std::uintptr_t id){std::lock_guard<std::mutex> lock(mutex);sinks.erase(id);}
    void Send(void* cookie,SoundCallbackEvent event) noexcept {
        try {
            Sink sink;
            {std::lock_guard<std::mutex> lock(mutex);
                const auto it=sinks.find(reinterpret_cast<std::uintptr_t>(cookie));if(it!=sinks.end())sink=it->second;}
            if(sink)sink(event);
        }catch(...){} // never unwind into the audio engine
    }
};
void __fastcall Began(void* cookie,std::uint32_t id,void*) noexcept {
    Dispatch::Get().Send(cookie,{SoundCallbackKind::Began,id,false});
}
void __fastcall Ended(void* cookie,std::uint32_t id,std::uint32_t flags,void* info,void*) noexcept {
    if(flags==1)Dispatch::Get().Send(cookie,{SoundCallbackKind::Ended,id,info!=nullptr});
}
bool Write(void* object,std::size_t offset,void* value) {
    return native::CopyMemoryChecked(static_cast<unsigned char*>(object)+offset,&value,sizeof(value));
}
}
struct SoundCallbacks::State {
    SoundCallbackAllocation allocation;ManagedRoots roots;Sink sink;
    std::vector<ManagedRootId> retained;
    std::uintptr_t cookie=0;
    SoundCallbackObjects objects;
    bool attempted=false,closed=false;
    State(SoundCallbackAllocation a,ReferenceMethods r,Sink s):allocation(std::move(a)),roots(r),sink(std::move(s)){}
    void Disconnect(){if(cookie){Dispatch::Get().Remove(cookie);cookie=0;}}
    ObjectResult Allocate(const SoundDelegateAllocation& a,void* entry) {
        if(!a.allocate)return {};
        const auto result=a.allocate();if(!result.call.returned || !result.object)return result;
        const auto root=roots.Keep(result.object);retained.push_back(root);
        if(!roots.Status(root).ready)return {};
        if(!Write(result.object,a.layout.method_ptr,entry) || !Write(result.object,a.layout.invoke_impl,entry) ||
            !Write(result.object,a.layout.method_code,reinterpret_cast<void*>(cookie)))return {};
        return result;
    }
};
SoundCallbacks::SoundCallbacks(SoundCallbackAllocation a,ReferenceMethods r,Sink sink)
    :state_(new State(std::move(a),r,std::move(sink))){}
SoundCallbacks::~SoundCallbacks(){state_->Disconnect();delete state_;}
SoundCallbackObjects SoundCallbacks::Create() {
    auto& s=*state_;if(s.closed)return {};
    if(s.attempted)return s.objects;
    s.attempted=true;if(!s.sink)return {};
    s.cookie=Dispatch::Get().Add(s.sink);
    s.objects.began=s.Allocate(s.allocation.began,reinterpret_cast<void*>(Began));
    if(s.objects.began.call.returned && s.objects.began.object)
        s.objects.ended=s.Allocate(s.allocation.ended,reinterpret_cast<void*>(Ended));
    if(!s.objects.began.object || !s.objects.ended.object){s.Disconnect();s.objects={};}
    return s.objects;
}
bool SoundCallbacks::Close() {
    auto& s=*state_;s.closed=true;s.Disconnect();s.objects={};
    for(auto it=s.retained.begin();it!=s.retained.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.retained.erase(it);}else ++it;
    }
    return s.retained.empty();
}
}
