# Build HksrApi

Requirements: Windows x64, an MSVC developer terminal, Windows SDK, CMake 3.25+ and Ninja.

```powershell
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release
cmake --install out/release
cpack --config out/release/CPackConfig.cmake -C Release -B out/packages
```

The preset builds in `out/release` and installs to `out/install`. First configuration downloads the commits in [dependencies.json](dependencies.json); see [dependency overrides](docs/dependencies.md).

## Outputs

- `bin/Hksr.Api.dll`, `lib/Hksr.Api.lib` and public headers.
- CMake packages `HksrApi` and `HksrApiNative`.
- Documentation, package metadata and licenses.
- With testing enabled, binary-consumer checks under `checks/abi4`.

CPack produces a Release ZIP named `HksrApi-<version>-win64-msvc<toolset>-Release.zip`. Use separate install directories for Debug and Release.

## Consume the package

```cmake
find_package(HksrApi 4.7.0 CONFIG REQUIRED COMPONENTS abi4)
target_link_libraries(MyMod PRIVATE Hksr::Api)
```

Set `CMAKE_PREFIX_PATH` to the extracted package. Follow the [C++ contract](docs/cpp-interface.md) for CRT and toolchain settings. Adapter tools can also link [NativeSupport](docs/native-support.md).

## Tests and CI

CTest covers runtime clients, capability ownership, metadata, native calls, binary loading and installed-package consumption using local substitutes.

GitHub Actions builds, tests and uploads the development ZIP as a workflow artifact.
