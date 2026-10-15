#include <hksr/Interface.hpp>
#include "../../src/metadata/MetadataCache.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {void Check(bool value){if(!value)throw std::runtime_error("metadata cache concurrency failed");}}
int main()try {
    hksr::api::detail::MetadataCache<int,int> cache;
    cache.Seed(1,10);
    Check(cache.Get(1,[]()->int{throw std::runtime_error("cached query reloaded");})==10);
    // Native loading may reenter another lookup; no lock spans that call.
    Check(cache.Get(2,[&]{return cache.Get(1,[]{return 0;})+1;})==11);
    bool failed=false;
    try{cache.Get(3,[]()->int{throw std::runtime_error("read failed");});}catch(const std::runtime_error&){failed=true;}
    Check(failed && cache.Get(3,[]{return 12;})==12);
    std::promise<void> entered,release;
    auto ready=entered.get_future();auto permission=release.get_future();
    int first=0;
    std::thread worker([&]{first=cache.Get(4,[&]{entered.set_value();permission.get();return 41;});});
    ready.get();
    const auto second=cache.Get(4,[]{return 42;});
    release.set_value();worker.join();
    Check(second==42 && first==42 && cache.Get(4,[]{return 43;})==42);
    std::cout<<"Metadata cache short-lock publication, reentry, failed load and concurrent miss passed; no game.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
