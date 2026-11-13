#include <hksr/presentation/attachments/AttachmentScale.hpp>
#include <map>
#include <iostream>
#include <stdexcept>
using namespace hksr::api;
int main() {
    try {
        std::map<std::string, AttachmentScaleValue> values;
        bool alive = true, fail = false, closed = false;
        int restores = 0;
        const auto check = [](bool ok) {
            if (!ok)
                throw std::runtime_error("attachment scale regression");
        };
        AttachmentScale session({[&] { return alive; },
                                 [&](const auto& key, auto value, bool, std::uint64_t) { values[key] = value; },
                                 [&](const auto& key) {
                                     if (fail)
                                         throw std::runtime_error("restore pending");
                                     values.erase(key);
                                     ++restores;
                                 },
                                 [&] {
                                     closed = true;
                                     return true;
                                 }});
        session.Set(1, "Arrow", {0, 0, 0});
        session.Set(2, "Arrow", {2, 2, 2});
        check(session.Release(1) && values.at("Arrow")[0] == 2 && restores == 0);
        fail = true;
        check(!session.Release(2) && !session.Error().empty());
        fail = false;
        check(session.Release(2) && values.empty() && restores == 1);
        session.Set(3, "Arrow", {0, 0, 0});
        session.Set(4, "Arrow", {2, 2, 2});
        check(session.Release(4) && values.at("Arrow")[0] == 0);
        check(session.Release(3) && values.empty());
        bool rejected = false;
        try {
            session.Set(5, "Arrow", {-1, 1, 1});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        session.Set(6, "Arrow", {0, 0, 0});
        alive = false;
        const auto before = restores;
        check(session.Close() && closed && restores == before && session.Close());
        {
            AttachmentScale* active = nullptr;
            bool stop_on_current = true;
            int writes = 0, cleanup = 0;
            AttachmentScale interrupted({[&] {
                                             if (stop_on_current) {
                                                 stop_on_current = false;
                                                 check(!active->Close());
                                             }
                                             return true;
                                         },
                                         [&](const auto&, auto, bool, std::uint64_t) { ++writes; }, [](const auto&) {},
                                         [&] {
                                             ++cleanup;
                                             return true;
                                         }});
            active = &interrupted;
            bool stopped = false;
            try {
                interrupted.Set(1, "Arrow", {0, 0, 0});
            } catch (const std::runtime_error&) {
                stopped = true;
            }
            check(stopped && writes == 0 && cleanup == 0 && interrupted.Close() && cleanup == 1);
        }
        std::cout << "attachment scale scopes passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
