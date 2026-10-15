#include "../../src/unity/MeshWriterLocator.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <iterator>

namespace {
using namespace hksr::sdk_v1::detail;
// Hand-assembled matching instructions plus NOP padding, never executable and
// not extracted from a game image. Addresses/layouts are deliberately synthetic.
struct Fixture {
    std::uint32_t wrapper_rva,writer_rva,bind_rva;
    std::vector<unsigned char> wrapper{
        0x44,0x8b,0x42,0x10,0x48,0x8b,0x12,0xe9,0,0,0,0,0xcc,0xcc,0xcc,0xcc,
        0x48,0x63,0x44,0x24,0x28,0x48,0x89,0x44,0x24,0x28,0xe9,0,0,0,0,0xcc};
    std::vector<unsigned char> writer = std::vector<unsigned char>(256,0x90);
    std::vector<unsigned char> bind{
        0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,
        0x48,0x83,0xec,0x20,0x49,0x63,0xf8,0x48,0x8b,0xea,0x48,0x8b,0xd9,0xe8,0,0,0,0,
        0x48,0x8b,0x73,0,0x48,0x8b,0xdf,0x48,0x8b,0x86,0,0,0,0,0x48,0xd1,0xe8,0x48,0x3b,0xf8,0x76,0,
        0x41,0xb8,0x01,0,0,0,0x48,0x8d,0x8e,0,0,0,0,0x48,0x8b,0xd3,0xe8,0,0,0,0,
        0x48,0x8b,0x8e,0,0,0,0,0x48,0x8b,0xd5,0x48,0x89,0x9e,0,0,0,0,0x48,0xc1,0xe3,0x06,0x4c,0x8b,0xc3};
    static void U32(std::vector<unsigned char>& bytes,std::size_t offset,std::uint32_t value) {
        std::memcpy(bytes.data()+offset,&value,sizeof(value));
    }
    Fixture(std::uint32_t w=0x1000,std::uint32_t t=0x3000,std::uint32_t b=0x5000,
        unsigned char mesh=0x38,std::uint32_t data=0x200):wrapper_rva(w),writer_rva(t),bind_rva(b) {
        const auto delta=static_cast<std::int32_t>(static_cast<std::int64_t>(t)-w-12);
        std::memcpy(wrapper.data()+8,&delta,sizeof(delta));
        const unsigned char arguments[]{0x41,0x8b,0xf8,0x48,0x8b,0xf2,0x48,0x8b,0xd9,0xe8,0,0,0,0};
        std::copy(std::begin(arguments),std::end(arguments),writer.begin()+32);
        const unsigned char load[]{0x48,0x8b,0x43,mesh};
        std::copy(std::begin(load),std::end(load),writer.begin()+160);
        bind[37]=mesh;
        U32(bind,44,data+24); U32(bind,65,data); U32(bind,80,data); U32(bind,90,data+16);
    }
    std::vector<ExecutableBytes> Sections() const {
        return {{wrapper.data(),wrapper.size(),wrapper_rva},{bind.data(),bind.size(),bind_rva},
            {writer.data(),writer.size(),writer_rva}};
    }
};
void Check(bool value) {if(!value)throw std::runtime_error("synthetic mesh writer relation failed");}
void Reject(const std::vector<ExecutableBytes>& sections,const char* expected) {
    try { LocateMeshWriters(sections); }
    catch(const std::runtime_error& e) { Check(std::string(e.what()).find(expected)!=std::string::npos);return; }
    throw std::runtime_error("invalid mesh writer relation accepted");
}
}
void TestMeshWriterLocator() {
    for(const auto& sample:{Fixture{},Fixture{0x9000,0x3100,0x7000,0x48,0x340}}) {
        const auto found=LocateMeshWriters(sample.Sections());
        Check(found.fixed_wrapper==sample.wrapper_rva && found.weights==sample.writer_rva && found.bindposes==sample.bind_rva);
        Check(found.mesh_data==sample.bind[37]);
        Check(found.bindpose_count==found.bindpose_data+16 && found.bindpose_capacity==found.bindpose_data+24);
    }
    Fixture base;
    auto duplicate=base.Sections();duplicate.push_back(duplicate.front());Reject(duplicate,"candidates");
    auto truncated=base.Sections();truncated.back().size=255;Reject(truncated,"leaves executable");
    auto variable=base;variable.wrapper[16]=0;Reject(variable.Sections(),"adjacent");
    auto pointer=base;Fixture::U32(pointer.bind,80,0x400);Reject(pointer.Sections(),"data/count/capacity");
    auto count=base;Fixture::U32(count.bind,90,0x220);Reject(count.Sections(),"data/count/capacity");
    auto capacity=base;Fixture::U32(capacity.bind,44,0x228);Reject(capacity.Sections(),"data/count/capacity");
    auto arguments=base;arguments.writer[32]=0;Reject(arguments.Sections(),"argument flow");
    auto duplicate_args=base;
    std::copy(base.writer.begin()+32,base.writer.begin()+46,duplicate_args.writer.begin()+64);
    Reject(duplicate_args.Sections(),"argument flow");
    auto layout=base;layout.writer[163]^=1;Reject(layout.Sections(),"native Mesh layouts");
    auto duplicate_load=base;
    std::copy(base.writer.begin()+160,base.writer.begin()+164,duplicate_load.writer.begin()+180);
    Reject(duplicate_load.Sections(),"native Mesh layouts");
    auto target=base;Fixture::U32(target.wrapper,8,0x100000);Reject(target.Sections(),"leaves executable");
    auto negative=base;Fixture::U32(negative.wrapper,8,0x80000000);Reject(negative.Sections(),"leaves RVA");
    std::cout<<"Synthetic mesh writer tests passed: relocation, negative rel32, field relations, ambiguity and bounds; no game bytes.\n";
}
