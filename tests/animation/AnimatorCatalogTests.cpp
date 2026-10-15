#include "../../adapters/animation/AnimatorTable.hpp"
#include <array>
#include <iostream>

namespace {
using namespace hksr::sdk_v1::detail;
void Check(bool ok){if(!ok)throw std::runtime_error("Animator table test failed");}
struct Blob {
    std::array<unsigned char,2048> bytes{};
    std::uintptr_t base=0x100000;
    template<class T>void Put(std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof value);}
    void RelativeAt(std::size_t slot,std::size_t target){Put<std::int64_t>(slot,static_cast<std::int64_t>(target)-static_cast<std::int64_t>(slot));}
    TableRead Reader()const{return [this](std::uintptr_t at,void* out,std::size_t size){
        if(at<base || at-base>bytes.size() || size>bytes.size()-(at-base))return false;
        std::memcpy(out,bytes.data()+at-base,size);return true;
    };}
    Blob() {
        // Root at 32, two layers, two machines. Offset arrays intentionally
        // include backward pointers and do not match serialized file layout.
        Put<std::uint32_t>(32,2);RelativeAt(40,160);
        Put<std::uint32_t>(48,2);RelativeAt(56,176);
        RelativeAt(160,128);RelativeAt(168,136);
        Put<std::uint32_t>(128,0);Put<std::uint32_t>(136,1);
        RelativeAt(176,224);RelativeAt(184,256);
        Put<std::uint32_t>(224,3);RelativeAt(232,288);
        Put<std::uint32_t>(256,1);RelativeAt(264,320);
        RelativeAt(288,512);RelativeAt(296,640);RelativeAt(304,768);RelativeAt(320,896);
        for(std::size_t i=0;i<4;++i) {
            auto n=512+i*128;
            Put<int>(n+0x30,static_cast<int>(10+i));Put<int>(n+0x34,static_cast<int>(110+i));Put<int>(n+0x38,static_cast<int>(210+i));
            Put<unsigned char>(n+0x5a,i==1?1:0);
        }
        // Same short name on another layer: do not deduplicate by short hash.
        Put<int>(896+0x30,10);
    }
};
}
void TestAnimatorCatalog() {
    Blob blob;
    auto read=[&]{return ReadAnimatorTable(blob.Reader(),blob.base+32,2);};
    auto out=read();Check(out.complete && out.layers.size()==2 && out.states.size()==4);
    Check(out.states[1].loop && !out.states[0].loop && out.states[3].layer==1);
    Check(out.states[0].short_hash==out.states[3].short_hash && out.states[0].full_path_hash!=out.states[3].full_path_hash);
    blob.base=0x720000000000;out=read();Check(out.complete && out.states[2].full_path_hash==212); // relocation
    blob.Put<std::uint32_t>(136,0);out=read();Check(out.complete && out.states.size()==6); // synchronized layer
    blob.Put<std::uint32_t>(136,2);out=read();Check(!out.complete && out.states.size()==3); // independent first layer retained
    blob.Put<std::uint32_t>(136,1);blob.Put<std::uint32_t>(224,0xffffffffu);
    out=read();Check(!out.complete && out.states.size()==1 && out.states[0].layer==1); // no huge count loop
    blob.Put<std::uint32_t>(224,0);out=read();Check(out.complete && out.states.size()==1); // empty layer is valid
    Check(!ReadAnimatorTable(blob.Reader(),blob.base+32,3).complete);
    bool overflow=false;try{TableAdd(UINTPTR_MAX,1);}catch(const std::exception&){overflow=true;}Check(overflow);
    std::cout<<"Animator table: relocated relative arrays, all states/layers, duplicate names, synchronized/empty layers, independent failures passed; game qualification still required.\n";
}
