#include <hksr/presentation/movement/AnimationMovementData.hpp>
#include <hksr/presentation/movement/AnimationMovementAccess.hpp>
#include <hksr/presentation/movement/AnimationMovementUpdates.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_name==owner && f.owner_namespace.empty() && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10) || f.offset<16)
            throw std::invalid_argument(std::string("movement field differs: ")+owner+"."+name);
        found=&f;
    }
    if(!found)throw std::invalid_argument(std::string("movement field missing: ")+owner+"."+name);
    return found->offset;
}
}
AnimationMoveDataLayout ResolveAnimationMoveDataLayout460(const std::vector<DeclaredField>& data,
    const std::vector<DeclaredField>& segment,const std::vector<DeclaredField>& sample) {
    // DDF is an inline 32-byte value, unlike the two reference objects. Metadata
    // reports boxed offsets; its unboxed position/percentage/distance/direction
    // layout is also consumed by the engine's sample evaluator.
    if(Field(sample,"DDFDNJCFIHM","LJCENKGKELP","RPG.MVector3")!=16 ||
       Field(sample,"DDFDNJCFIHM","HJLOCBLHNLD","System.Single")!=28 ||
       Field(sample,"DDFDNJCFIHM","GNPIPAADIFK","System.Single")!=32 ||
       Field(sample,"DDFDNJCFIHM","AIPNOJPDPCM","RPG.MVector3")!=36)
        throw std::invalid_argument("movement sample value layout differs");
    auto d=[&](const char* n,const char* t){return Field(data,"FGNNHFLGJBK",n,t);};
    auto s=[&](const char* n,const char* t){return Field(segment,"BCNMLILBENC",n,t);};
    return {d("JONGMMPJEIA","System.Collections.Generic.List<BCNMLILBENC>"),
        d("PIMDCJGDHMG","System.Single"),d("CNHMBFIHLPC","System.Single"),d("CDJLMJDEPJA","System.Single"),
        d("NOIIIOKMIIN","System.Int32"),d("GJGKKECHEHH","System.Single"),d("KPHIFEFPPDD","System.Boolean"),
        s("KLOKABEPKLL","RPG.GameCore.FloatCurve"),s("CKIBNLPPIMG","System.Single"),s("FJBPOFJPFNP","RPG.MVector3"),
        s("AAKHOJDBHPH","DDFDNJCFIHM"),s("FMHMAHCNDGB","System.Single"),
        s("HGDKNBJHDAP","RPG.GameCore.MoveToPositionUpdateMode"),s("NNNAGHMNFKB","System.Single")};
}
AnimationMoveDataServices ResolveAnimationMoveData460(RuntimeMetadata& metadata) {
    if(metadata.Version()!=RuntimeVersion::Hsr460)throw std::invalid_argument("animation movement data qualified for HSR 4.6");
    const auto data=metadata.FindType("","FGNNHFLGJBK"),segment=metadata.FindType("","BCNMLILBENC");
    AnimationMoveDataServices out;
    out.references=ResolveGcMethods(metadata);
    out.layout=ResolveAnimationMoveDataLayout460(metadata.ReadFields(data),metadata.ReadFields(segment),metadata.ReadFields("","DDFDNJCFIHM"));
    const auto fields=metadata.ObserveFields(data);RuntimeType list;
    for(const auto& f:fields.fields)if(f.declaration.name=="JONGMMPJEIA")list=metadata.FieldType(f.field);
    if(!list)throw std::invalid_argument("movement segment list type unavailable");
    out.data=[&metadata,data]{return metadata.AllocateObject(data);};
    out.segment=[&metadata,segment]{return metadata.AllocateObject(segment);};
    out.list=[&metadata,list]{return metadata.AllocateObject(list);};
    out.data_ctor=metadata.Resolve(data,".ctor","System.Void",{});
    out.segment_ctor=metadata.Resolve(segment,".ctor","System.Void",{});
    out.list_ctor=metadata.Resolve(list,".ctor","System.Void",{"System.Int32"});
    out.list_add=metadata.Resolve(list,"Add","System.Void",{"BCNMLILBENC"});
    return out;
}
AnimationMovementAccessBinding ResolveAnimationMovementAccess460(MethodResolver& methods,
    const std::vector<DeclaredField>& fields,AnimationMoveDataLayout graph) {
    auto f=[&](const char* n,const char* t){return Field(fields,"BGOBCKLPEMK",n,t);};
    std::size_t owner=0;
    for(const auto& row:fields)if(row.owner_namespace=="RPG.GameCore" && row.owner_name=="GameComponentBase" && row.name=="_OwnerRef") {
        if(owner || row.type!="RPG.GameCore.GameEntity" || !row.flags_observed || (row.flags&0x10) || row.offset<16)
            throw std::invalid_argument("movement component owner declaration differs");
        owner=row.offset;
    }
    if(!owner)throw std::invalid_argument("movement component owner declaration missing");
    return {methods.Resolve("","BGOBCKLPEMK","KDGCMELNKDA","System.Void",{"FGNNHFLGJBK"}),
        methods.Resolve("","BGOBCKLPEMK","HMNPNOIKFFH","System.Void",{}),owner,
        f("KILJAPEKFAJ","RPG.GameCore.TransformComponent"),f("CMGGMAGPMEA","IBEJMEEEJMB"),f("MIHFBGNKBJA","AKDDNBHNGON"),
        f("PGALAIJOICA","FGNNHFLGJBK"),f("<IsAttachMotionWithEffect>k__BackingField","System.Boolean"),
        f("MFKFCJBNHEK","System.Boolean"),f("JDKEKGODFLL","System.Int32"),graph};
}
AnimationMovementAccessBinding ResolveAnimationMovementAccess460(RuntimeMetadata& metadata) {
    if(metadata.Version()!=RuntimeVersion::Hsr460)throw std::invalid_argument("animation movement access qualified for HSR 4.6");
    return ResolveAnimationMovementAccess460(metadata,metadata.ReadFields("","BGOBCKLPEMK"),
        ResolveAnimationMoveDataLayout460(metadata.ReadFields("","FGNNHFLGJBK"),metadata.ReadFields("","BCNMLILBENC"),metadata.ReadFields("","DDFDNJCFIHM")));
}
Method ResolveAnimationMovementUpdate460(MethodResolver& methods) {
    return methods.Resolve("","BGOBCKLPEMK","FFLMBFFEHLC","System.Boolean",{"System.Single","RPG.MVector3&"});
}
}
