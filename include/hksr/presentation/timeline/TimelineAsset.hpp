#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../managed/References.hpp"
#include <memory>
namespace hksr::sdk_v1 {
struct TimelineTrackInfo {
    std::string type;
    bool muted=false;
    int markers=0;
};
struct TimelineAssetInfo {
    std::vector<TimelineTrackInfo> tracks;
    bool complete=false;
    std::string error;
};
// Resource data only: no Director, graph creation, playback or shared mutation.
// Calls on the official engine thread; returned data contains no game pointers.
class HKSR_API TimelineAssetReader {
public:
    TimelineAssetReader(MethodResolver&,ManagedObjects&,ObjectMetadata&,ReferenceMethods);
    ~TimelineAssetReader();
    TimelineAssetReader(const TimelineAssetReader&)=delete;
    TimelineAssetReader& operator=(const TimelineAssetReader&)=delete;
    TimelineAssetInfo Inspect(const std::string& path);
private:
    struct State;State* state_;
};
}
