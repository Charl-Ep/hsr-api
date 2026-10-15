#include <hksr/world/WorldMap.hpp>
#include <exception>

namespace hksr::sdk_v1 {
WorldMapMethods ResolveWorldMapMethods(MethodResolver& source) {
    WorldMapMethods out;
    auto get=[&](const char* owner,const char* name,const char* result){
        try{return source.Resolve("RPG.Client",owner,name,result,{});}
        catch(const std::exception& error){
            out.binding_errors.push_back(std::string(owner)+"."+name+": "+error.what());return Method{};
        }
    };
    out.group_manager=get("AdventurePhase","get_RuntimeGroupManager","RPG.Client.RuntimeGroupManager");
    out.map=get("RuntimeGroupManager","GetMapDef","RPG.Client.MapDef");
    out.pending_switch=get("AdventurePhase","get_PendingSwitchScene","System.Boolean");
    out.runtime_id=get("MapDef","get_MapRuntimeID","System.UInt64");
    const char* names[]={"get_PlaneID","get_FloorID","get_DimensionID","get_ContentID","get_StoryLineID","get_CurrentGameMode"};
    for(std::size_t i=0;i<out.identity.size();++i)
        out.identity[i]=get("MapDef",names[i],i==5?"RPG.GameCore.GameModeType":"System.UInt32");
    return out;
}
}
