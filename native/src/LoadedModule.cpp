#include <hksr/native/LoadedModule.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
namespace hksr::api::native {
bool LoadedModule::Executable(void* address) const {
    const auto at = reinterpret_cast<std::uintptr_t>(address), start = reinterpret_cast<std::uintptr_t>(base);
    if (at < start || at - start >= size)
        return false;
    const auto rva = at - start;
    for (const auto& s : sections)
        if ((s.Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= s.VirtualAddress &&
            rva - s.VirtualAddress < s.Misc.VirtualSize)
            return true;
    return false;
}
LoadedModule ReadModule(const wchar_t* name) {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(name));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    // Protection-region boundaries are not PE-header boundaries. Checked
    // reads allow a valid header to span regions; the image extent below
    // validates the actual PE structure rather than a page-size assumption.
    if (!base || !CopyMemoryChecked(&dos, base, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 ||
        static_cast<std::size_t>(dos.e_lfanew) > UINTPTR_MAX - reinterpret_cast<std::uintptr_t>(base) ||
        !CopyMemoryChecked(&nt, base + dos.e_lfanew, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        throw std::runtime_error("loaded engine PE header is absent or invalid");
    const auto size = nt.OptionalHeader.SizeOfImage;
    if (!size || nt.OptionalHeader.SizeOfHeaders > size || static_cast<std::size_t>(dos.e_lfanew) > size ||
        sizeof(nt) > size - static_cast<std::size_t>(dos.e_lfanew))
        throw std::runtime_error("loaded engine PE extent is invalid");
    LoadedModule out{base, size, {}};
    out.timestamp = nt.FileHeader.TimeDateStamp;
    const auto offset = static_cast<std::size_t>(dos.e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                        nt.FileHeader.SizeOfOptionalHeader;
    const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (offset > size || bytes > size - offset)
        throw std::runtime_error("engine PE section table is outside image");
    out.sections.resize(nt.FileHeader.NumberOfSections);
    if (!CopyMemoryChecked(out.sections.data(), base + offset, bytes))
        throw std::runtime_error("engine PE sections are unreadable");
    for (const auto& s : out.sections)
        if (s.VirtualAddress > size || s.Misc.VirtualSize > size - s.VirtualAddress)
            throw std::runtime_error("engine PE section extent is outside image");
    return out;
}
}
