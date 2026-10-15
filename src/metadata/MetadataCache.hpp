#pragma once
#include <map>
#include <mutex>
#include <utility>

namespace hksr::sdk_v1::detail {
// Domain-local immutable results. Never hold a cache lock across native API
// calls: metadata queries can reenter callbacks or another cache. Concurrent
// misses may both read; the first published result wins for this provider.
template<class Key,class Value> class MetadataCache {
    std::mutex mutex_;
    std::map<Key,Value> entries_;
public:
    void Seed(Key key,Value value) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.emplace(std::move(key),std::move(value));
    }
    template<class Load> Value Get(const Key& key,Load&& load) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto found=entries_.find(key);if(found!=entries_.end())return found->second;
        }
        auto value=load();
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.emplace(key,std::move(value)).first->second;
    }
};
}
