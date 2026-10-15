#pragma once
#include "AnimationControl.hpp"
#include "../unity/ComponentConfig.hpp"
#include <functional>

namespace hksr::sdk_v1 {
struct ControllerState {
    int layer=0;
    std::uint32_t machine=0,index=0;
    int short_hash=0,path_hash=0,full_path_hash=0;
    std::string name,path,full_path;
    bool loop=false,recognized=false,name_roundtrip=false;
};
struct ControllerLayer {
    int index=0;
    std::string name;
    std::uint32_t machine=0,declared_states=0;
};
struct ControllerCatalog {
    bool complete=false;
    std::vector<ControllerLayer> layers;
    std::vector<ControllerState> states;
    std::vector<std::string> issues;
};
// Read the actual initialized Animator, not a dictionary of guessed names or
// clip names. No play/transition calls, new Animator, or controller replacement.
// Result owns values only. Caller supplies an owned live model on its engine
// thread. Full state coverage does NOT imply full task/ability orchestration.
class HKSR_API AnimatorCatalog {
public:
    explicit AnimatorCatalog(RuntimeMetadata& metadata);
    ~AnimatorCatalog();
    AnimatorCatalog(const AnimatorCatalog&)=delete;
    AnimatorCatalog& operator=(const AnimatorCatalog&)=delete;
    ControllerCatalog Read(void* animator);
private:
    struct State;
    State* state_;
};
}
