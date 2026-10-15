#include "../../../src/presentation/events/AnimationConfigSchema.hpp"
#include <iostream>
#include <stdexcept>
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
namespace {
void Check(bool v){if(!v)throw std::runtime_error("animation config schema test failed");}
struct Types:TypeRelationships {
    std::set<std::string> tasks{"TaskConfig","CameraTask","Sequence","Switch"};
    std::map<std::string,std::vector<DeclaredField>> fields;
    unsigned reads=0;
    bool TypeDerivesFrom(const char* ns,const char* name,const char*,const char* base)override {
        Check(std::string(ns)=="RPG.GameCore");
        if(std::string(base)=="ValueType")return std::string(name)=="Enum" || std::string(name)=="DynamicFloat";
        Check(std::string(base)=="TaskConfig");return tasks.count(name)!=0;
    }
    void Field(const char* owner,const char* name,const char* type) {fields[owner].push_back({"RPG.GameCore",owner,name,type,128,6,true});}
};
}
void TestAnimationConfigSchema() {
    Types types;
    types.Field("SingleAnimEventConfig","EventList","RPG.GameCore.TaskConfig[]");
    types.Field("SingleAnimEventConfig","StateName","System.String");
    types.Field("Sequence","TaskList","RPG.GameCore.TaskConfig[]");
    types.Field("Switch","CaseTaskList","RPG.GameCore.SwitchCaseTaskList[]");
    types.Field("Switch","DefaultTaskList","RPG.GameCore.TaskConfig[]");
    types.Field("Switch","Mode","RPG.GameCore.Enum");
    types.Field("SwitchCaseTaskList","Case","RPG.GameCore.DynamicFloat");
    types.Field("SwitchCaseTaskList","TaskList","RPG.GameCore.TaskConfig[]");
    types.Field("CameraTask","TaskEnabled","System.Boolean");
    // A newly named carrier is identified by structure without adding a name.
    types.Field("Switch","Other","RPG.GameCore.NewWrapper[]");
    types.Field("NewWrapper","Recursive","RPG.GameCore.NewWrapper");
    types.Field("NewWrapper","Peer","RPG.GameCore.Peer");
    types.Field("Peer","Back","RPG.GameCore.NewWrapper");
    types.Field("Peer","Task","RPG.GameCore.CameraTask");
    AnimationConfigSchema schema(types,[&](const char*,const char* name){++types.reads;return types.fields.at(name);},{"RPG.GameCore.CameraTask"});
    auto describe=[&](const char* name){return schema.Describe(nullptr,{"RPG.GameCore",name,{}});};
    Check(describe("SingleAnimEventConfig").edges.size()==1);
    Check(describe("Switch").edges.size()==3);
    Check(describe("SwitchCaseTaskList").edges.size()==1);
    Check(describe("NewWrapper").edges.size()==2 && describe("Peer").edges.size()==2);
    const auto camera=describe("CameraTask");Check(camera.disable_task && camera.enabled.offset==128 && camera.edges.empty());
    const auto reads=types.reads;Check(describe("Switch").edges.size()==3 && types.reads==reads);
    Check(!describe("Sequence").disable_task);
    types.tasks.insert("Unsupported");types.Field("Unsupported","Tasks","System.Collections.Generic.List<RPG.GameCore.TaskConfig>");
    bool failed=false;try{describe("Unsupported");}catch(const std::exception&){failed=true;}Check(failed);
    std::cout<<"Animation config schema passed: declared task paths, unnamed carriers, cycles, value boundaries and per-domain cache. Synthetic declarations only.\n";
}
