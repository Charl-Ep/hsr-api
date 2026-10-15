#include <hksr/presentation/effects/EffectCompletion.hpp>
#include <array>
#include <map>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace {
using namespace hksr::api;
std::map<int,void*> roots;int serial=0,info,effect,parameters;
bool retain_failure=false;
void Check(bool value){if(!value)throw std::runtime_error("effect completion bridge test failed");}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),&info};}
int __fastcall Keep(void* object,int,int,void*){if(retain_failure)return 0;roots[++serial]=object;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
// Deliberately not the game's numeric offsets. Emulate its qualified direct
// invocation (impl, unmanaged cookie, two arguments, ignored method metadata).
void Invoke(void* object,const EffectCompletionLayout& layout,void* instance,void* input) {
    void* entry=nullptr;void* cookie=nullptr;
    std::memcpy(&entry,static_cast<char*>(object)+layout.invoke_impl,sizeof(entry));
    std::memcpy(&cookie,static_cast<char*>(object)+layout.method_code,sizeof(cookie));
    reinterpret_cast<void(__fastcall*)(void*,void*,void*,void*)>(entry)(cookie,instance,input,nullptr);
}
}
void TestEffectCompletion() {
    std::vector<DeclaredField> fields{
        {"System","Delegate","method_ptr","System.IntPtr",32,1,true},
        {"System","Delegate","invoke_impl","System.IntPtr",48,1,true},
        {"System","Delegate","method_code","System.IntPtr",96,1,true}};
    const auto layout=ResolveEffectCompletionLayout460(fields);
    auto rejected=[&] {try{ResolveEffectCompletionLayout460(fields);return false;}catch(const std::exception&){return true;}};
    fields[0].flags=0x10;Check(rejected());fields[0].flags=1;
    fields.push_back(fields.front());Check(rejected());fields.pop_back();
    fields[1].offset=32;Check(rejected());fields[1].offset=48;
    const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    alignas(void*) std::array<unsigned char,128> delegate{};
    int allocations=0,callbacks=0;
    EffectCompletionAllocation allocation{[&]{++allocations;delegate.fill(0);return ObjectResult{{true,0},delegate.data()};},layout};
    {
        EffectCompletion bridge(allocation,refs,[&](EffectCompletionEvent event){
            Check(event.effect==&effect && event.parameters==&parameters);++callbacks;
        });
        const auto created=bridge.Create();Check(created.call.returned && created.object==delegate.data() && roots.size()==1);
        Check(bridge.Create().object==created.object && allocations==1);
        Invoke(created.object,layout,&effect,&parameters);Check(callbacks==1);
        Check(bridge.Close() && bridge.Close() && roots.empty());
        // Simulate a managed holder retaining the old delegate after Close.
        Invoke(created.object,layout,&effect,&parameters);Check(callbacks==1);
        Check(!bridge.Create().call.returned);
    }
    {
        EffectCompletion* reentrant=nullptr;
        EffectCompletion bridge(allocation,refs,[&](EffectCompletionEvent event){
            Check(!event.effect && event.parameters==&parameters);++callbacks;Check(reentrant->Close());
        });reentrant=&bridge;
        const auto created=bridge.Create();Invoke(created.object,layout,nullptr,&parameters);
        Check(callbacks==2 && roots.empty());Invoke(created.object,layout,nullptr,&parameters);Check(callbacks==2);
    }
    {
        EffectCompletion bridge(allocation,refs,[&](EffectCompletionEvent){++callbacks;throw std::runtime_error("consumer failure");});
        const auto created=bridge.Create();Invoke(created.object,layout,nullptr,&parameters);
        Check(callbacks==3 && bridge.Close()); // exception does not cross the engine call
    }
    {
        retain_failure=true;
        EffectCompletion bridge(allocation,refs,[](EffectCompletionEvent){});
        Check(!bridge.Create().call.returned && bridge.Close());retain_failure=false;
    }
    Check(roots.empty());
    std::cout<<"Effect completion bridge: relocated declared fields, direct value ABI, reentrant close, inert late callback and exception isolation passed; no game invocation.\n";
}
