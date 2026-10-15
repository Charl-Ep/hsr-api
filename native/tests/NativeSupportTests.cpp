#include <hksr/native/Invocation.hpp>
#include <hksr/native/LoadedModule.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool value) { if(!value) throw std::runtime_error("native support contract failed"); }
int metadata,object,calls;
float __fastcall Mixed(void* self,int amount,float scale,void* method) {
    Check(self==&object && method==&metadata);++calls;return amount*scale;
}
void __fastcall Write(int value,void* method) {
    Check(method==&metadata);object=value;++calls;
    RaiseException(0xE047AA01,0,0,nullptr);
}
}
int main() try {
    using namespace hksr::api;
    float value=73;
    Check(!native::Call({},&value,&object,3,0.5f).returned && value==73 && !calls);
    const auto result=native::Call({reinterpret_cast<void*>(&Mixed),&metadata},&value,&object,3,0.5f);
    Check(result.returned && value==1.5f && calls==1);
    const auto partial=native::CallVoid({reinterpret_cast<void*>(&Write),&metadata},42);
    Check(!partial.returned && partial.exception==0xE047AA01 && object==42 && calls==2);
    // The native boundary reports a partial write. No rollback or retry.
    Check(native::CopyNativeString("declaration")=="declaration");
    int copied=0;Check(native::CopyMemoryChecked(&copied,&object,sizeof(copied)) && copied==42);
    Check(!native::CopyMemoryChecked(&copied,reinterpret_cast<void*>(1),sizeof(copied)));
    // Constructor accepts a candidate table, does not connect to the game.
    native::NativeMetadataReader absent({});
    Check(!absent.ObserveTypes().errors.empty());
    // Validate only this test executable; no game module is loaded or called.
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto image=native::ReadModule(nullptr,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage);
    Check(image.Executable(reinterpret_cast<void*>(&Mixed)) && !image.Executable(&object));
    bool rejected=false;
    try{native::ReadModule(nullptr,nt->FileHeader.TimeDateStamp^1,nt->OptionalHeader.SizeOfImage);}
    catch(const std::runtime_error&){rejected=true;}
    Check(rejected);
    std::cout<<"Native support linked without runtime DLL: exact managed ABI, partial write receipt, memory and module reads.\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
