#include <hksr/presentation/attachments/AttachmentVisibility.hpp>
#include <map>
#include <stdexcept>
#include <iostream>
using namespace hksr::api;
int main() {
    try {
        std::map<std::string, bool> values{{"Weapon", true}, {"Hidden", false}};
        bool alive = true, closed = false, fail = false;
        int calls = 0;
        AttachmentVisibility visibility({[&] { return alive; }, [&](const auto& key) { return values.at(key); },
                                         [&](const auto& key, bool value, int) {
                                             values[key] = value;
                                             ++calls;
                                             if (fail)
                                                 throw std::runtime_error("native fault");
                                         },
                                         [&] {
                                             closed = true;
                                             return true;
                                         }});
        const auto need = [](bool value) {
            if (!value)
                throw std::runtime_error("attachment visibility regression");
        };
        visibility.Set(1, "Weapon", false);
        visibility.Set(2, "Weapon", true);
        need(visibility.Release(1) && values.at("Weapon"));
        need(visibility.Release(2) && values.at("Weapon")); // no stale hidden baseline
        visibility.Set(3, "Weapon", false);
        visibility.Set(4, "Weapon", true);
        need(visibility.Release(4) && !values.at("Weapon"));
        need(visibility.Release(3) && values.at("Weapon"));
        visibility.Set(5, "Hidden", true);
        values["Hidden"] = false;
        const auto before = calls;
        need(visibility.Release(5) && calls == before && !values.at("Hidden"));
        visibility.Set(8, "Weapon", false);
        visibility.Set(9, "Weapon", true);
        values["Weapon"] = false;
        need(visibility.Release(9) && !values.at("Weapon"));
        const auto external = calls;
        need(visibility.Release(8) && calls == external && !values.at("Weapon"));
        values["Weapon"] = true;
        fail = true;
        try {
            visibility.Set(6, "Weapon", false);
            need(false);
        } catch (const std::runtime_error&) {
        }
        fail = false;
        need(visibility.Release(6) && values.at("Weapon"));
        visibility.Set(7, "Weapon", false);
        alive = false;
        const auto ended = calls;
        need(visibility.Close() && closed && calls == ended);
        need(visibility.Close());
        {
            AttachmentVisibility* active = nullptr;
            bool stop_on_current = true;
            int writes = 0, cleanup = 0;
            AttachmentVisibility interrupted({[&] {
                                                  if (stop_on_current) {
                                                      stop_on_current = false;
                                                      need(!active->Close());
                                                  }
                                                  return true;
                                              },
                                              [](const auto&) { return true; },
                                              [&](const auto&, bool, int) { ++writes; },
                                              [&] {
                                                  ++cleanup;
                                                  return true;
                                              }});
            active = &interrupted;
            bool stopped = false;
            try {
                interrupted.Set(1, "Weapon", false);
            } catch (const std::runtime_error&) {
                stopped = true;
            }
            need(stopped && writes == 0 && cleanup == 0 && interrupted.Close() && cleanup == 1);
        }
        std::cout << "attachment visibility scopes passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
