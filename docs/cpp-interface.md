# C++ interface

HksrApi uses a Windows x64 C++ DLL interface. Consumers use C++17+, compatible MSVC tools, default structure packing, exceptions and the shared CRT.

## Package contract

```cmake
find_package(HksrApi 4.6.2 CONFIG REQUIRED COMPONENTS abi1)
target_link_libraries(MyMod PRIVATE Hksr::Api)
```

Build with headers and import libraries from the same package. The imported target includes generated `hksr/CppAbi.hpp`, which checks architecture, CRT and STL iterator settings.

| SDK package | Consumer |
|---|---|
| Release | `/MD`, `_ITERATOR_DEBUG_LEVEL=0` |
| Debug | `/MDd`, `_ITERATOR_DEBUG_LEVEL=2` |

RelWithDebInfo and MinSizeRel can use a Release package. Use separate install directories for CRT variants. Non-CMake consumers must include the generated contract and link the matching import library explicitly.

## Ownership

STL values and callbacks cross the DLL boundary under this toolchain contract. Returned references are borrowed unless documented otherwise. A copied game-object pointer does not retain the object.

Copyable call handles such as `EntityAccess`, `AnimationControl` and `MeshAccess` share immutable SDK binding storage. Their construction, copying, assignment and destruction execute in the DLL. Rvalue copies leave the source usable. `ActorControl::BindMono` replaces only that handle's bindings.

Resource owners and sessions use explicit cleanup on the official thread. Keep owners, callbacks and their modules alive until cleanup finishes. Destructors do not replace engine cleanup. DLL hot-unloading is unsupported.

## Versioning

SDK versions use `game-major.game-minor.sdk-revision`. The current SDK is 4.6.2 for game 4.6.0.

Interface generation 1 uses namespace `hksr::sdk_v1`, aliased as `hksr::api`. The CMake component is `abi1`. Compatible revisions preserve public value layouts, signatures and ownership. Incompatible changes require a new generation and consumer rebuild.

Ordinary clients obtain capabilities from `PresentationContext`. Explicit binding tables, method-table constructors, declaration resolvers and `Hksr::NativeSupport` require the exact SDK package used at runtime. They are adapter/test interfaces, outside the capability-level replacement contract.

## Consumer checks

Build the independent installed-package consumer:

```powershell
cmake -S tests/package -B out/package-test -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/sdk"
cmake --build out/package-test
out/package-test/consumer.exe
```

Negative compile options are `HKSR_TEST_CRT=MultiThreaded`, `HKSR_TEST_CRT=MultiThreadedDebugDLL` against Release, and `HKSR_TEST_ITERATOR_LEVEL=1`.

## Binary consumer checks

Packages built with testing enabled include a compiled consumer and runners in `checks/abi1`. Preserve the released consumer DLL. Test upgrades using the new package's runner and SDK with the unchanged consumer:

```powershell
$env:PATH = "C:/new-sdk/bin;$env:PATH"
& C:/new-sdk/checks/abi1/AbiCapabilitiesTests.exe C:/baseline-sdk/checks/abi1/AbiConsumer.dll
& C:/new-sdk/checks/abi1/AbiLoadTests.exe C:/baseline-sdk/checks/abi1/AbiConsumer.dll C:/new-sdk/bin/Hksr.Api.dll
```

The capability runner supplies synthetic bindings and checks copied handles, model/resource reads, factory lifetime and independent session cleanup. The loader checks symbol compatibility. Game integration is tested separately.
