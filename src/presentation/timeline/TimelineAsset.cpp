#include <hksr/presentation/timeline/TimelineAsset.hpp>
#include <hksr/unity/AssetLeases.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/native/Invocation.hpp>
#include <sstream>

namespace hksr::sdk_v1 {
struct TimelineAssetReader::State {
    ManagedObjects& objects;ObjectMetadata& metadata;
    AssetLeases assets;ManagedRoots roots;
    Method flattened,muted,marker_count;
    std::vector<ManagedRootId> held;
    std::vector<AssetLeaseId> leases;
    State(MethodResolver& m,ManagedObjects& o,ObjectMetadata& d,ReferenceMethods refs)
        :objects(o),metadata(d),assets(refs),roots(refs),
        flattened(m.Resolve("UnityEngine.Timeline","TimelineAsset","get_flattenedTracks","System.Collections.Generic.IEnumerable<UnityEngine.Timeline.TrackAsset>",{})),
        muted(m.Resolve("UnityEngine.Timeline","TrackAsset","get_mutedInHierarchy","System.Boolean",{})),
        marker_count(m.Resolve("UnityEngine.Timeline","TrackAsset","GetMarkerCount","System.Int32",{})){}
    void* Keep(ObjectResult result) {
        if(!result.call.returned || !result.object)throw std::runtime_error("Timeline resource reference unavailable");
        held.reserve(held.size()+1);const auto id=roots.Keep(result.object);held.push_back(id);
        const auto target=roots.Target(id);
        if(!target.call.returned || !target.object)throw std::runtime_error("Timeline resource reference retention failed");
        return target.object;
    }
    std::string Close() {
        std::ostringstream error;
        for(auto i=leases.begin();i!=leases.end();) {
            if(assets.Release(*i).complete){assets.Forget(*i);i=leases.erase(i);}
            else {error<<"asset release unresolved "<<*i<<"; ";++i;}
        }
        for(auto i=held.begin();i!=held.end();) {
            if(roots.Release(*i).complete){roots.Forget(*i);i=held.erase(i);}
            else {error<<"GC release unresolved "<<*i<<"; ";++i;}
        }
        return error.str();
    }
};
TimelineAssetReader::TimelineAssetReader(MethodResolver& m,ManagedObjects& o,ObjectMetadata& d,ReferenceMethods r)
    :state_(new State(m,o,d,r)){}
TimelineAssetReader::~TimelineAssetReader(){
    // No engine calls during destruction. Retain any uncertain cleanup receipts
    // for the process lifetime rather than discard them or retry a decrement.
    if(state_->leases.empty() && state_->held.empty())delete state_;
}
TimelineAssetInfo TimelineAssetReader::Inspect(const std::string& path) {
    TimelineAssetInfo out;auto& s=*state_;
    try {
        if(path.empty())throw std::runtime_error("Timeline resource path is empty");
        auto* name=s.Keep(s.objects.NewStringUtf8(path.c_str()));
        auto* type=s.Keep(s.objects.ManagedType("UnityEngine.Timeline","TimelineAsset"));
        s.leases.reserve(s.leases.size()+1);const auto lease=s.assets.Acquire(name,type,false);s.leases.push_back(lease);
        auto* asset=s.assets.Object(lease);
        if(!asset)throw std::runtime_error("Timeline resource load failed: "+path);
        const auto actual=s.metadata.ReadObjectType(asset);
        if(actual.name_space!="UnityEngine.Timeline" || actual.name!="TimelineAsset")throw std::runtime_error("loaded resource is not TimelineAsset");
        ObjectResult collection;collection.call=native::Call(s.flattened,&collection.object,asset);auto* list=s.Keep(collection);
        ManagedReferenceList tracks(ResolveReferenceListMethods(s.metadata.ReadObjectType(list),"UnityEngine.Timeline.TrackAsset"));
        const auto count=tracks.Count(list);
        if(!count.read.returned || count.value<0)throw std::runtime_error("Timeline track count unread");
        for(int i=0;i<count.value;++i) {
            auto* track=s.Keep(tracks.Item(list,i));const auto declared=s.metadata.ReadObjectType(track);
            TimelineTrackInfo row;row.type=declared.name_space+"."+declared.name;
            if(!native::Call(s.muted,&row.muted,track).returned || !native::Call(s.marker_count,&row.markers,track).returned || row.markers<0)
                throw std::runtime_error("Timeline track flags/markers unread: "+row.type);
            out.tracks.push_back(std::move(row));
        }
        out.complete=true;
    }catch(const std::exception& e){out.error=e.what();}
    catch(...){out.error="Timeline resource native read exception";}
    const auto cleanup=s.Close();
    if(!cleanup.empty()){out.complete=false;out.error+="; "+cleanup;}
    return out;
}
}
