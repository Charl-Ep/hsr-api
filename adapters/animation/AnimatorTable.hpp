#pragma once
#include <hksr/animation/AnimatorCatalog.hpp>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
// Native self-relative constant arrays. Layout is established from the 4.6
// GetAnimatorStateInfo writer, not from serialized asset offsets. Callers must
// qualify that writer before applying this view to game memory.
using TableRead=std::function<bool(std::uintptr_t,void*,std::size_t)>;
inline std::uintptr_t TableAdd(std::uintptr_t base,std::int64_t delta) {
    if(delta>=0) {
        if(static_cast<std::uint64_t>(delta)>UINTPTR_MAX-base)throw std::runtime_error("table address overflow");
        return base+static_cast<std::uintptr_t>(delta);
    }
    const auto magnitude=static_cast<std::uint64_t>(-(delta+1))+1;
    if(magnitude>base)throw std::runtime_error("table address underflow");
    return base-static_cast<std::uintptr_t>(magnitude);
}
template<class T>T TableValue(const TableRead& read,std::uintptr_t address) {
    T result{};
    if(!address || !read(address,&result,sizeof result))throw std::runtime_error("unreadable controller table field");
    return result;
}
inline std::uintptr_t Relative(const TableRead& read,std::uintptr_t slot) {
    const auto offset=TableValue<std::int64_t>(read,slot);
    if(!offset)throw std::runtime_error("null relative controller entry");
    return TableAdd(slot,offset);
}
inline std::uintptr_t TableEntry(const TableRead& read,std::uintptr_t array,std::uint32_t index) {
    const auto count=TableValue<std::uint32_t>(read,array);
    if(index>=count)throw std::runtime_error("controller table index outside declared count");
    return Relative(read,TableAdd(Relative(read,TableAdd(array,8)),static_cast<std::int64_t>(index)*8));
}
// Each layer is independent; one unreadable layer cannot erase other rows.
// Counts come from the actual table; there is no six-actor/four-action cap.
inline ControllerCatalog ReadAnimatorTable(const TableRead& read,std::uintptr_t root,int expected_layers) {
    ControllerCatalog out;
    try {
        const auto count=TableValue<std::uint32_t>(read,root);
        if(expected_layers<0 || count!=static_cast<unsigned>(expected_layers))throw std::runtime_error("native/official layer counts differ");
        for(std::uint32_t layer=0;layer<count;++layer) {
            try {
                const auto layer_node=TableEntry(read,root,layer);
                const auto machine=TableValue<std::uint32_t>(read,layer_node);
                const auto states=TableEntry(read,TableAdd(root,16),machine);
                const auto state_count=TableValue<std::uint32_t>(read,states);
                out.layers.push_back({static_cast<int>(layer),{},machine,state_count});
                // Validate the final pointer slot before count-driven walking;
                // an unreadable/malformed span must not trigger a huge loop.
                if(state_count)TableValue<std::int64_t>(read,TableAdd(Relative(read,states+8),static_cast<std::int64_t>(state_count-1)*8));
                for(std::uint32_t i=0;i<state_count;++i) {
                    try {
                        const auto node=TableEntry(read,states,i);
                        ControllerState row;row.layer=static_cast<int>(layer);row.machine=machine;row.index=i;
                        row.short_hash=TableValue<int>(read,TableAdd(node,0x30));
                        row.path_hash=TableValue<int>(read,TableAdd(node,0x34));
                        row.full_path_hash=TableValue<int>(read,TableAdd(node,0x38));
                        row.loop=TableValue<unsigned char>(read,TableAdd(node,0x5a))!=0;
                        out.states.push_back(std::move(row));
                    }catch(const std::exception& e){out.issues.push_back("layer "+std::to_string(layer)+" state "+std::to_string(i)+": "+e.what());}
                }
            }catch(const std::exception& e){out.issues.push_back("layer "+std::to_string(layer)+": "+e.what());}
        }
    }catch(const std::exception& e){out.issues.push_back(e.what());}
    out.complete=out.issues.empty();return out;
}
}
