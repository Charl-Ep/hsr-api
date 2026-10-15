#pragma once
#include <map>
#include <string>
#include <cstdint>

namespace hksr::sdk_v1::detail {
// Suppress repeated diagnostic text only, never events or control flow. Keep
// occurrence counts so a quiet log is not mistaken for a recovered condition.
class RepeatedDiagnostics {
public:
    bool Publish(const std::string& text) {
        bool diagnostic=false;
        for(const auto* word:{"failed","unresolved","unknown","incomplete","unreadable","absent","unavailable"})
            if(text.find(word)!=std::string::npos){diagnostic=true;break;}
        if(!diagnostic)return true;
        auto& count=occurrences_[text];++count;
        if(count==1)return true;
        ++suppressed_;return false;
    }
    std::uint64_t Suppressed() const{return suppressed_;}
    std::uint64_t Count(const std::string& text) const {
        const auto i=occurrences_.find(text);return i==occurrences_.end()?0:i->second;
    }
private:
    std::map<std::string,std::uint64_t> occurrences_;
    std::uint64_t suppressed_=0;
};
}
