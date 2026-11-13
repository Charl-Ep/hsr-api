#include "../../src/presentation/materials/FaceMaterialComponents.hpp"
#include <iostream>

namespace {
using namespace hksr::api;
using namespace hksr::api::detail;
void Check(bool value) {
    if (!value)
        throw std::runtime_error("face material initialization test failed");
}
template <class F> void Fails(F run) {
    bool failed = false;
    try {
        run();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Check(failed);
}
}
int main() {
    try {
        int component = 0, creates = 0, initializes = 0;
        bool alive = true, present = false, lookup_ok = true, create_ok = true, end_in_create = false,
             init_fail = false;
        FaceMaterialComponentServices services{
            [&] { return alive; }, [&] { return ObjectResult{{lookup_ok, 0}, present ? &component : nullptr}; },
            [&] {
                ++creates;
                if (end_in_create)
                    alive = false;
                if (create_ok)
                    present = true;
                return ObjectResult{{create_ok, 0}, create_ok ? &component : nullptr};
            },
            [&](void* value) {
                Check(value == &component);
                ++initializes;
                if (init_fail)
                    throw std::runtime_error("native model callback failed");
            }};
        Check(EnsureFaceMaterialComponent(services));
        Check(creates == 1 && initializes == 1 && present);
        Check(!EnsureFaceMaterialComponent(services)); // Existing engine component is untouched.
        Check(creates == 1 && initializes == 1);
        present = false;
        lookup_ok = false;
        Fails([&] { EnsureFaceMaterialComponent(services); });
        Check(creates == 1);
        lookup_ok = true;
        create_ok = false;
        Fails([&] { EnsureFaceMaterialComponent(services); });
        Check(creates == 2 && initializes == 1);
        create_ok = true;
        end_in_create = true;
        Fails([&] { EnsureFaceMaterialComponent(services); });
        Check(creates == 3 && initializes == 1);
        end_in_create = false;
        alive = true;
        present = false;
        init_fail = true;
        Fails([&] { EnsureFaceMaterialComponent(services); });
        Check(creates == 4 && initializes == 2);
        alive = false;
        Fails([&] { EnsureFaceMaterialComponent(services); });
        Check(creates == 4);
        std::cout << "Owned face material component setup passed; no game acceptance.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
