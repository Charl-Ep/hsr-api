#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>

namespace hksr::api::native {
struct LoadedModule {
    unsigned char* base = nullptr;
    std::size_t size = 0;
    std::vector<IMAGE_SECTION_HEADER> sections;
    bool Executable(void* address) const;
    DWORD timestamp = 0;
};
// Reads an already loaded module only. Never LoadLibrary or attach a process.
LoadedModule ReadModule(const wchar_t* name);
}
