#include "MeshWriterLocator.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace hksr::sdk_v1::detail {
namespace {
using Pattern=std::vector<int>;
const Pattern fixed{0x44,0x8b,0x42,0x10,0x48,0x8b,0x12,0xe9,-1,-1,-1,-1,0xcc,0xcc,0xcc,0xcc};
const Pattern variable{0x48,0x63,0x44,0x24,0x28,0x48,0x89,0x44,0x24,0x28,0xe9,-1,-1,-1,-1,0xcc};
const Pattern bindpose{
    0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,
    0x48,0x83,0xec,0x20,0x49,0x63,0xf8,0x48,0x8b,0xea,0x48,0x8b,0xd9,0xe8,-1,-1,-1,-1,
    0x48,0x8b,0x73,-1,0x48,0x8b,0xdf,0x48,0x8b,0x86,-1,-1,-1,-1,0x48,0xd1,0xe8,0x48,0x3b,-1,0x76,-1,
    0x41,0xb8,0x01,0x00,0x00,0x00,0x48,0x8d,0x8e,-1,-1,-1,-1,0x48,0x8b,0xd3,0xe8,-1,-1,-1,-1,
    0x48,0x8b,0x8e,-1,-1,-1,-1,0x48,0x8b,0xd5,0x48,0x89,0x9e,-1,-1,-1,-1,0x48,0xc1,0xe3,0x06,0x4c,0x8b,0xc3};
const Pattern arguments{0x41,0x8b,0xf8,0x48,0x8b,-1,0x48,0x8b,0xd9,0xe8,-1,-1,-1,-1};
bool Matches(const unsigned char* bytes,std::size_t size,const Pattern& p) {
    if(size<p.size()) return false;
    for(std::size_t i=0;i<p.size();++i) if(p[i]>=0 && bytes[i]!=p[i]) return false;
    return true;
}
std::vector<std::uint32_t> Find(const std::vector<ExecutableBytes>& sections,const Pattern& p) {
    std::vector<std::uint32_t> result;
    for(const auto& s:sections) {
        if(!s.data || s.size<p.size()) continue;
        std::size_t offset=0;
        while(offset<=s.size-p.size()) {
            const auto* hit=static_cast<const unsigned char*>(std::memchr(s.data+offset,p[0],s.size-p.size()-offset+1));
            if(!hit) break;
            offset=static_cast<std::size_t>(hit-s.data);
            if(Matches(hit,s.size-offset,p)) result.push_back(s.rva+static_cast<std::uint32_t>(offset));
            ++offset;
        }
    }
    return result;
}
const unsigned char* Read(const std::vector<ExecutableBytes>& sections,std::uint32_t rva,std::size_t size) {
    for(const auto& s:sections) if(rva>=s.rva && rva-s.rva<=s.size && size<=s.size-(rva-s.rva)) return s.data+(rva-s.rva);
    throw std::runtime_error("mesh writer relation leaves executable image bytes");
}
std::uint32_t U32(const unsigned char* bytes) { std::uint32_t value=0; std::memcpy(&value,bytes,4); return value; }
}
MeshWriterLocations LocateMeshWriters(const std::vector<ExecutableBytes>& sections) {
    const auto f=Find(sections,fixed), b=Find(sections,bindpose);
    if(f.size()!=1 || b.size()!=1) throw std::runtime_error("mesh writer candidates: fixed="+std::to_string(f.size())+", bindpose="+std::to_string(b.size()));
    MeshWriterLocations out; out.fixed_wrapper=f[0]; out.bindposes=b[0];
    const auto* wrapper=Read(sections,f[0],32);
    if(!Matches(wrapper+16,16,variable)) throw std::runtime_error("adjacent variable-weight wrapper differs");
    std::int32_t displacement=0; std::memcpy(&displacement,wrapper+8,4);
    const auto target=static_cast<std::int64_t>(f[0])+12+displacement;
    if(target<0 || target>UINT32_MAX) throw std::runtime_error("fixed weight jump leaves RVA space");
    out.weights=static_cast<std::uint32_t>(target);
    const auto* writer=Read(sections,out.weights,256);
    const auto* bind=Read(sections,out.bindposes,bindpose.size());
    out.mesh_data=bind[37]; out.bindpose_capacity=U32(bind+44); out.bindpose_data=U32(bind+65); out.bindpose_count=U32(bind+90);
    if(out.bindpose_data!=U32(bind+80) || static_cast<std::uint64_t>(out.bindpose_data)+16!=out.bindpose_count ||
       static_cast<std::uint64_t>(out.bindpose_data)+24!=out.bindpose_capacity)
        throw std::runtime_error("bindpose data/count/capacity relation differs");
    const std::vector<ExecutableBytes> prefix{{writer,256,out.weights}};
    if(Find(prefix,arguments).size()!=1) throw std::runtime_error("fixed weight argument flow differs");
    std::size_t data_loads=0;
    for(std::size_t i=0;i+4<=256;++i)
        if(writer[i]==0x48 && writer[i+1]==0x8b && (writer[i+2]==0x43 || writer[i+2]==0x73) && writer[i+3]==out.mesh_data) ++data_loads;
    if(data_loads!=1) throw std::runtime_error("fixed weight and bindpose native Mesh layouts differ");
    return out;
}
}
