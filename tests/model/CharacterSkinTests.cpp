#include "../../src/model/CharacterSkinNative.hpp"
#include <iostream>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
template <class F> void Reject(F call, const char* reason) {
    bool rejected = false;
    try {
        call();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, reason);
}
}
int main() try {
    CharacterSkinStatus status;
    status.prepared = true;
    status.skin_id = 1131001;
    status.avatar_id = 1310;
    status.adventure_model = "skin/adventure";
    status.battle_model = "skin/battle";
    unsigned mappings = 0, sounds = 0, closes = 0;
    bool release_ready = false;
    auto skin = detail::CharacterSkinFactory::Create(
        status,
        [&](const std::string& path) -> std::optional<std::string> {
            ++mappings;
            if (path == "suppressed")
                return {};
            if (path == "broken")
                throw std::runtime_error("official mapping failed");
            return path == "base" ? "skin" : path;
        },
        [&](const std::string& name) {
            ++sounds;
            return name + "_Skin01";
        },
        [&] {
            ++closes;
            return release_ready;
        });
    Check(skin->Status().prepared && skin->Status().skin_id == 1131001 && skin->Status().avatar_id == 1310,
          "skin identity changed");
    Check(skin->MapResource("base") == "skin" && skin->MapResource("skin") == "skin",
          "resource mapping changed or was applied twice");
    Check(!skin->MapResource("suppressed"), "official null suppression became a default/empty resource");
    Reject([&] { skin->MapResource("broken"); }, "mapping failure was hidden as suppression");
    Check(skin->MapResource("") == "" && mappings == 4, "empty resource caused an unnecessary official query");
    Check(skin->SelectSound("sound") == "sound_Skin01" && sounds == 1,
          "explicit animation sound selection not forwarded");
    Check(skin->SelectSound("").empty() && sounds == 1, "empty sound caused an official lookup");
    Reject([&] { skin->MapResource(std::string("a\0b", 3)); }, "resource with embedded NUL was truncated");
    auto second = skin;
    skin.reset();
    Check(second->MapResource("base") == "skin" && closes == 0,
          "shared preparation closed when one instance released it");
    Check(!second->Close() && !second->Status().closed, "incomplete release reported closed");
    release_ready = true;
    Check(second->Close() && second->Status().closed && !second->Status().prepared, "explicit release did not close");
    Check(second->Close() && closes == 2, "closed skin repeated managed cleanup");
    Reject([&] { second->MapResource("base"); }, "closed mapper called official code");
    Reject([&] { second->SelectSound("sound"); }, "closed sound selector called official code");
    CharacterSkinStatus failed;
    failed.errors = {"one preparation failed"};
    auto unavailable = detail::CharacterSkinFactory::Create(
        failed, [](const auto& p) { return std::optional<std::string>(p); }, [](const auto& p) { return p; },
        [] { return true; });
    Reject([&] { unavailable->MapResource("base"); }, "unprepared skin allowed mapping");
    Check(unavailable->Close(), "failed preparation could not release");
    unsigned destructor_closes = 0;
    {
        auto value = detail::CharacterSkinFactory::Create(
            status, [](const auto& p) { return std::optional<std::string>(p); }, [](const auto& p) { return p; },
            [&] {
                ++destructor_closes;
                return true;
            });
    }
    Check(!destructor_closes, "native destructor made engine calls");
    std::cout
        << "Skin identity, mapping/null/error separation, shared ownership, close and native destruction passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
