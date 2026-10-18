#pragma once
#include "ManagedConfigProjection.hpp"
#include <map>
#include <set>

namespace hksr::sdk_v1::detail {
// Follows DECLARATION relationships to tasks, not runtime entity references.
// Rank-one reference arrays and configuration records. TaskConfig/JsonConfig
// descendants may live outside RPG.GameCore, including the empty namespace.
// Value types, strings, Unity objects and generic containers are not traversed.
// Generic containers of TaskConfig are reported as unsupported, not silently
// represented as an empty task list. No names/offsets from a captured instance.
class AnimationConfigSchema {
public:
    using ReadFields=std::function<std::vector<DeclaredField>(const char*,const char*)>;
    using Disable=std::function<bool(void*,const DeclaredType&,const std::vector<DeclaredField>&)>;
    AnimationConfigSchema(TypeRelationships& relationships,ReadFields read,std::set<std::string> disabled,Disable conditional={})
        :relationships_(relationships),read_(std::move(read)),disabled_(std::move(disabled)),conditional_(std::move(conditional)) {}
    ConfigObjectShape Describe(void* object,const DeclaredType& type) {
        const auto name=type.name_space+"."+type.name;
        if(!HasTasks(name))throw std::runtime_error("event configuration object has no declared task path: "+name);
        ConfigObjectShape out;
        const bool disable=disabled_.count(name) || (conditional_ && conditional_(object,type,Fields(name)));
        for(const auto& field:Fields(name)) {
            if(!field.flags_observed)throw std::runtime_error("configuration field flags unavailable: "+name+"."+field.name);
            if(field.flags&0x10)continue;
            if(HasTasks(field.type))out.edges.push_back(field);
            if(disable && field.name=="TaskEnabled" && field.type=="System.Boolean") {
                if(out.disable_task)throw std::runtime_error("ambiguous configuration TaskEnabled: "+name);
                out.disable_task=true;out.enabled=field;
            }
        }
        if(disable && !out.disable_task)throw std::runtime_error("configuration TaskEnabled absent: "+name);
        return out;
    }
private:
    TypeRelationships& relationships_;ReadFields read_;std::set<std::string> disabled_;
    Disable conditional_;
    std::map<std::string,std::vector<DeclaredField>> fields_;
    std::map<std::string,bool> reaches_task_;
    std::map<std::string,bool> candidates_,tasks_;
    static std::string Element(std::string type) {
        if(type.size()>2 && type.compare(type.size()-2,2,"[]")==0)type.resize(type.size()-2);
        if(type.find('.')==std::string::npos)type.insert(0,".");
        return type;
    }
    bool Derives(const std::string& name,const char* ns,const char* base) {
        const auto dot=name.rfind('.');
        return relationships_.TypeDerivesFrom(name.substr(0,dot).c_str(),name.substr(dot+1).c_str(),ns,base);
    }
    bool Task(const std::string& name) {
        const auto found=tasks_.find(name);if(found!=tasks_.end())return found->second;
        return tasks_.emplace(name,Derives(name,"RPG.GameCore","TaskConfig")).first->second;
    }
    const std::vector<DeclaredField>& Fields(const std::string& name) {
        const auto found=fields_.find(name);if(found!=fields_.end())return found->second;
        const auto dot=name.rfind('.');
        return fields_.emplace(name,read_(name.substr(0,dot).c_str(),name.substr(dot+1).c_str())).first->second;
    }
    bool Candidate(const std::string& name) {
        if(name.find('<')!=std::string::npos) {
            if(name.find("RPG.GameCore.TaskConfig")!=std::string::npos)
                throw std::runtime_error("unsupported task configuration container: "+name);
            return false;
        }
        if(name.find_first_of("[]&*")!=std::string::npos)return false;
        const auto found=candidates_.find(name);if(found!=candidates_.end())return found->second;
        const bool candidate=Task(name) ||
            ((name.compare(0,13,"RPG.GameCore.")==0 || Derives(name,"RPG.GameCore","JsonConfig")) &&
             !Derives(name,"System","ValueType"));
        return candidates_.emplace(name,candidate).first->second;
    }
    bool HasTasks(const std::string& field_type) {
        const auto root=Element(field_type);
        auto prior=reaches_task_.find(root);if(prior!=reaches_task_.end())return prior->second;
        if(!Candidate(root)){reaches_task_[root]=false;return false;}
        // Do not cache a recursive false before a cycle's other branch has
        // been visited. Discover once, then propagate from task nodes upwards.
        std::map<std::string,std::vector<std::string>> parents;
        std::set<std::string> seen,positive;
        std::vector<std::string> pending{root};seen.insert(root);
        for(std::size_t i=0;i<pending.size();++i) {
            const auto current=pending[i];
            auto cached=reaches_task_.find(current);
            if(cached!=reaches_task_.end()) {if(cached->second)positive.insert(current);continue;}
            if(Task(current)) {
                positive.insert(current);continue;
            }
            for(const auto& field:Fields(current)) {
                if(!field.flags_observed)throw std::runtime_error("configuration field flags unavailable: "+current+"."+field.name);
                if(field.flags&0x10)continue;
                const auto child=Element(field.type);if(!Candidate(child))continue;
                parents[child].push_back(current);
                if(seen.insert(child).second)pending.push_back(child);
            }
        }
        std::vector<std::string> changed(positive.begin(),positive.end());
        for(std::size_t i=0;i<changed.size();++i)
            for(const auto& parent:parents[changed[i]])if(positive.insert(parent).second)changed.push_back(parent);
        for(const auto& type:pending)reaches_task_[type]=positive.count(type)!=0;
        return reaches_task_.at(root);
    }
};
}
