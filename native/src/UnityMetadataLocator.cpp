#include <hksr/native/UnityMetadataLocator.hpp>
#include <cstring>
#include <limits>

namespace hksr::sdk_v1::native {
std::vector<MetadataTableLocation> LocateUnityMetadataTables(
    const std::vector<MetadataCodeSection>& code, std::size_t image_size,
    const ReadImageBytes& read) {
    constexpr char domain[]="IL2CPP Root Domain";
    constexpr std::size_t observed_table_bytes=171*8;
    std::vector<MetadataTableLocation> found;
    for(const auto& s:code) {
        if(!s.data || s.size<16 || s.rva>image_size || s.size>image_size-s.rva) continue;
        for(std::size_t i=0;i<=s.size-16;++i) {
            const auto* p=s.data+i;
            if(p[0]!=0x48 || p[1]!=0x8b || p[2]!=0x05 ||
               p[7]!=0x48 || p[8]!=0x8d || p[9]!=0x0d || p[14]!=0xff || p[15]!=0xd0) continue;
            const auto site=static_cast<std::int64_t>(s.rva)+static_cast<std::int64_t>(i);
            std::int32_t table_delta=0,name_delta=0;
            std::memcpy(&table_delta,p+3,4);std::memcpy(&name_delta,p+10,4);
            const auto table=site+7+table_delta,name=site+14+name_delta;
            auto within=[image_size](std::int64_t start,std::size_t bytes) {
                return start>=0 && static_cast<std::uint64_t>(start)<=std::numeric_limits<std::uint32_t>::max()
                    && static_cast<std::uint64_t>(start)<=image_size && bytes<=image_size-static_cast<std::size_t>(start);
            };
            if(!within(table,observed_table_bytes) || table%8 || !within(name,sizeof(domain))) continue;
            char actual[sizeof(domain)]{};
            if(!read(static_cast<std::uint32_t>(name),actual,sizeof(actual)) || std::memcmp(actual,domain,sizeof(domain))) continue;
            found.push_back({static_cast<std::uint32_t>(site),static_cast<std::uint32_t>(table),static_cast<std::uint32_t>(name)});
        }
    }
    return found;
}
}
