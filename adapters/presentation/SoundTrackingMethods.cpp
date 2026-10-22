#include <hksr/presentation/audio/SoundTracking.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
SoundTrackingBinding ResolveSoundTracking460(RuntimeMetadata& source) {
    if(source.Version()!=RuntimeVersion::Hsr460)throw std::runtime_error("audio request tracking qualified for 4.6 only");
    const auto audio=source.FindType("RPG.Client","AudioManager");
    const auto pending=source.FindType("","NCOONILMDAC"),playing=source.FindType("","ILKDFKIBOKA");
    auto field=[&](RuntimeType type,const char* name,const char* expected) {
        const auto rows=source.ObserveFields(type);
        for(const auto& row:rows.fields)if(row.declaration.name==name && row.error.empty() &&
            row.declaration.type==expected && row.declaration.flags_observed && !(row.declaration.flags&0x10))return row;
        throw std::runtime_error(std::string("audio field unavailable: ")+name);
    };
    const auto queue_field=field(audio,"_PendingAudioEvents","System.Collections.Generic.CacheLinkedList<NCOONILMDAC>");
    const auto queue=source.FieldType(queue_field.field);
    const auto first=source.Resolve(queue,"get_First","System.Collections.Generic.CacheLinkedList.JNMCPLGKILH<NCOONILMDAC>",{});
    const auto node=source.ReturnType(first);
    const auto cache=source.Resolve(audio,"get_AudioPlayingCacheData","DHCNPPICIAO",{});
    const auto ids=source.Resolve(source.ReturnType(cache),"HAJIJIJCDGM","System.Collections.Generic.List<System.UInt32>",{"System.UInt32"});
    const auto list=source.ReturnType(ids);
    SoundTrackingBinding out;
    out.methods={first,
        source.Resolve(node,"FJEPCCMFIJM","System.Collections.Generic.CacheLinkedList.JNMCPLGKILH<NCOONILMDAC>",{}),
        source.Resolve(node,"MNLDMBGAJIF","NCOONILMDAC",{}),
        source.Resolve(queue,"AALCFBOGFKP","System.Void",{"System.Collections.Generic.CacheLinkedList.JNMCPLGKILH<NCOONILMDAC>"}),
        source.Resolve(audio,"_RemoveSoundBankUsage","System.Void",{"System.Collections.Generic.List<System.UInt32>"}),
        cache,ids,source.Resolve(audio,"GetPlayingInfoByPlayingID","ILKDFKIBOKA",{"System.UInt32"}),
        source.Resolve(list,"get_Count","System.Int32",{}),source.Resolve(list,"get_Item","System.UInt32",{"System.Int32"}),
        source.Resolve("","AkSoundEngine","ExecuteActionOnPlayingID","System.Void",
            {"AkActionOnEventType","System.UInt32","System.Int32"},MethodKind::Static)};
    constexpr auto callback="System.Action<System.UInt32,AkCallbackType,AkCallbackInfo>";
    out.layout={queue_field.declaration.offset,
        field(pending,"PHNEEILABKK",callback).declaration.offset,
        field(pending,"OEDEAIGDAOF","UnityEngine.GameObject").declaration.offset,
        field(pending,"CFCEACHEPAB","System.Collections.Generic.List<System.UInt32>").declaration.offset,
        field(playing,"PHNEEILABKK",callback).declaration.offset,
        field(playing,"MEDICEEIEOE","UnityEngine.GameObject").declaration.offset};
    return out;
}
}
