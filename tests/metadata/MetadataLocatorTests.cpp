#include <hksr/native/UnityMetadataLocator.hpp>
#include "../../src/runtime/RuntimeInitialization.hpp"
#include <Windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::sdk_v1::native;
void Check(bool ok) {if(!ok) throw std::runtime_error("metadata locator contract mismatch");}
template<class T> T Read(const std::vector<unsigned char>& data,std::size_t offset) {
    Check(offset<=data.size() && sizeof(T)<=data.size()-offset);
    T value{};std::memcpy(&value,data.data()+offset,sizeof(value));return value;
}
void Inspect(const char* path) {
    std::ifstream in(std::filesystem::path(path),std::ios::binary|std::ios::ate);Check(in.good());
    const auto size=in.tellg();Check(size>0);
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),size);Check(in.good());
    const auto dos=Read<IMAGE_DOS_HEADER>(data,0);Check(dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>=0);
    const auto nt=Read<IMAGE_NT_HEADERS64>(data,dos.e_lfanew);
    Check(nt.Signature==IMAGE_NT_SIGNATURE && nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 && nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    std::vector<IMAGE_SECTION_HEADER> sections;
    std::vector<MetadataCodeSection> code;
    std::size_t offset=static_cast<std::size_t>(dos.e_lfanew)+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i,offset+=sizeof(IMAGE_SECTION_HEADER)) {
        auto s=Read<IMAGE_SECTION_HEADER>(data,offset);
        Check(s.PointerToRawData<=data.size() && s.SizeOfRawData<=data.size()-s.PointerToRawData);
        sections.push_back(s);
        if(s.Characteristics&IMAGE_SCN_MEM_EXECUTE) code.push_back({data.data()+s.PointerToRawData,s.SizeOfRawData,s.VirtualAddress});
    }
    auto read_image=[&](std::uint32_t rva,void* out,std::size_t bytes) {
        for(const auto& s:sections) if(rva>=s.VirtualAddress && rva-s.VirtualAddress<=s.SizeOfRawData && bytes<=s.SizeOfRawData-(rva-s.VirtualAddress)) {
            std::memcpy(out,data.data()+s.PointerToRawData+(rva-s.VirtualAddress),bytes);return true;
        }
        return false;
    };
    if(std::filesystem::path(path).filename()==L"GameAssembly.dll") {
        // Offline golden observations only. Production gets slot0 from the
        // live Unity table and checks resolved Windows imports, not these RVAs.
        const bool current=nt.FileHeader.TimeDateStamp==0x6AAAE554;
        Check(current || nt.FileHeader.TimeDateStamp==0x6A7CCA0A);
        const std::uint32_t entry=current?0x3cc0c10:0x3ca1230;
        const std::uint32_t site=current?0x3d44909:0x625009;
        unsigned char prologue[47]{};Check(read_image(site,prologue,sizeof(prologue)));
        std::int32_t de=0,dl=0;std::memcpy(&de,prologue+9,4);std::memcpy(&dl,prologue+41,4);
        std::uintptr_t enter=0,leave=0;
        Check(read_image(site+13+de,&enter,8) && read_image(site+45+dl,&leave,8));
        const auto found=hksr::api::detail::LocateRuntimeInitialization(entry,nt.OptionalHeader.SizeOfImage,read_image,enter,leave);
        Check(found && found->mutex_slot==(current?0x9aead50u:0x950eb50u) && found->count==(current?0x9aead78u:0x950eb78u));
        std::cout<<"Static runtime initialization synchronization resolved from root-init wrapper; no invocation.\n";
        return;
    }
    const auto found=LocateUnityMetadataTables(code,nt.OptionalHeader.SizeOfImage,read_image);
    std::cout<<"Static Unity metadata table candidates: "<<found.size()<<"; no invocation.\n";
    for(const auto& r:found) std::cout<<std::hex<<"  instruction="<<r.instruction<<", table="<<r.table<<", domain_name="<<r.domain_name<<std::dec<<'\n';
    Check(found.size()==1);
}
}
void TestMetadataLocator(const char* optional_image) {
    {
        std::vector<unsigned char> memory(0x1000);
        // Both retained versions use this same entry/exit synchronization.
        const unsigned char shape[]={0x48,0x8b,0x0d,0,0,0,0,0xff,0x15,0,0,0,0,0x8b,0x05,0,0,0,0,
            0x8d,0x48,1,0x89,0x0d,0,0,0,0,0x85,0xc0,0x7e,0x61,0x48,0x8b,0x0d,0,0,0,0,
            0xff,0x15,0,0,0,0,0xb0,1};
        memory[0x100]=0xe8;
        const std::int32_t call=0x300-0x105;std::memcpy(memory.data()+0x101,&call,4);
        std::memcpy(memory.data()+0x320,shape,sizeof(shape));
        auto rel=[&](unsigned at,unsigned end,unsigned target) {
            const auto value=static_cast<std::int32_t>(target-0x320-end);std::memcpy(memory.data()+0x320+at,&value,4);
        };
        rel(3,7,0x800);rel(35,39,0x800);rel(15,19,0x808);rel(24,28,0x808);
        rel(9,13,0x900);rel(41,45,0x908);
        const std::uintptr_t enter=0x1234,leave=0x5678;
        std::memcpy(memory.data()+0x900,&enter,8);std::memcpy(memory.data()+0x908,&leave,8);
        auto read=[&](std::uint32_t rva,void* out,std::size_t n){
            if(rva>memory.size() || n>memory.size()-rva)return false;
            std::memcpy(out,memory.data()+rva,n);return true;
        };
        auto locate=[&]{return hksr::api::detail::LocateRuntimeInitialization(0x100,memory.size(),read,enter,leave);};
        auto found=locate();Check(found && found->mutex_slot==0x800 && found->count==0x808);
        rel(35,39,0x810);Check(!locate());rel(35,39,0x800);
        rel(24,28,0x810);Check(!locate());rel(24,28,0x808);
        memory[0x908]=0;Check(!locate());std::memcpy(memory.data()+0x908,&leave,8);
        memory[0x320+46]=0;Check(!locate());
    }
    std::vector<unsigned char> image(0x3000);
    constexpr char name[]="IL2CPP Root Domain";
    std::memcpy(image.data()+0x80,name,sizeof(name));
    auto set=[&](std::uint32_t site,std::uint32_t table) {
        const unsigned char bytes[]={0x48,0x8b,0x05,0,0,0,0,0x48,0x8d,0x0d,0,0,0,0,0xff,0xd0};
        std::memcpy(image.data()+site,bytes,sizeof(bytes));
        const auto td=static_cast<std::int32_t>(table-site-7),nd=static_cast<std::int32_t>(0x80-site-14);
        std::memcpy(image.data()+site+3,&td,4);std::memcpy(image.data()+site+10,&nd,4);
    };
    set(0x1000,0x2000);
    std::vector<MetadataCodeSection> code{{image.data()+0x1000,16,0x1000}};
    auto read=[&](std::uint32_t rva,void* out,std::size_t bytes) {
        if(rva>image.size() || bytes>image.size()-rva) return false;
        std::memcpy(out,image.data()+rva,bytes);return true;
    };
    auto locate=[&]{return LocateUnityMetadataTables(code,image.size(),read);};
    auto first=locate();Check(first.size()==1 && first[0].table==0x2000 && first[0].domain_name==0x80);
    code[0].size=15;Check(locate().empty());code[0].size=16;
    image[0x80]='x';Check(locate().empty());image[0x80]='I';
    image[0x80+sizeof(name)-1]='x';Check(locate().empty());image[0x80+sizeof(name)-1]=0;
    set(0x1000,0x2f00);Check(locate().empty());set(0x1000,0x2001);Check(locate().empty());set(0x1000,0x2000);
    set(0x1100,0x2100);code.push_back({image.data()+0x1100,16,0x1100});Check(locate().size()==2);
    Check(LocateUnityMetadataTables(code,image.size(),[](std::uint32_t,void*,std::size_t){return false;}).empty());
    if(optional_image) Inspect(optional_image);
    std::cout<<"Metadata locator tests passed: signed relocations, literal/terminator, bounds and ambiguity retained.\n";
}
