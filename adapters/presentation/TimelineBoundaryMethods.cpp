#include "TimelineBoundaryMethods.hpp"

namespace hksr::sdk_v1::detail {
TimelineBoundaryMethods ResolveTimelineBoundaryMethods(MethodResolver& m) {
        const auto borrow=m.Resolve("","HPPNAAGGAMP","APGJAAKNKKI","UnityEngine.Playables.PlayableDirector",
            {"System.String","System.String","UnityEngine.Playables.PlayableAsset","UnityEngine.Vector3","UnityEngine.Vector3"});
        const auto returned=m.Resolve("","HPPNAAGGAMP","OFHBGOMIELI","System.Void",{"UnityEngine.Playables.PlayableDirector"});
        const auto removed=m.Resolve("","HPPNAAGGAMP","NKDINGGCODD","System.Void",{"EBMMGODFEEO"});
        const auto disposed=m.Resolve("","HPPNAAGGAMP","Dispose","System.Void",{});
        const auto stop=m.Resolve("","HPPNAAGGAMP","ADJGFHMINEF","System.Void",{"System.String"});
        const auto resource=m.Resolve("","HPPNAAGGAMP","IODNDPEJOFF","EBMMGODFEEO",{"System.String"});
        const auto playing=m.Resolve("","HPPNAAGGAMP","MOOCHPDGDFK","System.Boolean",{"System.String"});
        const auto recent=m.Resolve("","HPPNAAGGAMP","EKDLILNHAAN","System.Boolean",{"System.String","System.Int32"});
        const auto stop_record=m.Resolve("","EBMMGODFEEO","HFKFNFKIEON","System.Void",{});
    return {borrow,returned,removed,disposed,stop,resource,playing,recent,stop_record};
}
}
