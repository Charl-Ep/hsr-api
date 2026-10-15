#include <Windows.h>
#include <iostream>
#include <stdexcept>

void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv) try {
    Check(argc==3 || argc==4,"expected consumer and SDK paths, optionally an incompatible SDK");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    auto sdk=LoadLibraryW(argv[2]);Check(sdk!=nullptr,"load actual SDK");
    auto consumer=LoadLibraryW(argv[1]);Check(consumer!=nullptr,"load same-interface consumer");
    auto smoke=reinterpret_cast<bool(*)()>(GetProcAddress(consumer,"ConsumerSmoke"));
    Check(smoke && smoke(),"consumer value round trip");
    FreeLibrary(consumer);FreeLibrary(sdk);
    if(argc==3){std::cout<<"Existing consumer completed against the supplied SDK.\n";return 0;}
    Check(!GetModuleHandleW(L"Hksr.Api.dll"),"SDK unexpectedly resident after stopped-host read");
    auto wrong=LoadLibraryW(argv[3]);Check(wrong!=nullptr,"load incompatible fixture");
    SetLastError(0);consumer=LoadLibraryW(argv[1]);const auto error=GetLastError();
    if(consumer)FreeLibrary(consumer);FreeLibrary(wrong);
    Check(!consumer && error==ERROR_PROC_NOT_FOUND,"different interface must fail symbol resolution");
    std::cout<<"Actual SDK consumer loaded; incompatible interface rejected before consumer initialization.\n";
    return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
