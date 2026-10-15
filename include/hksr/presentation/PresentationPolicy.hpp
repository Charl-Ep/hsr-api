#pragma once

namespace hksr::sdk_v1 {
struct PresentationIsolationPolicy {
    bool allow_main_camera=false;
    bool allow_timeline_environment=false;
};
// Policy intent only; concrete game declarations are selected by the SDK.
struct AnimationTaskPolicy {
    bool allow_camera_noise=false;
    bool constrain_head_look_to_owner=true;
};
}
