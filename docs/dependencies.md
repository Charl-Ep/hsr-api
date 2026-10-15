# Dependencies

[dependencies.json](../dependencies.json) pins upstream commits. CMake FetchContent downloads them into the build directory and reuses the cache.

| Dependency | Use | Local source override |
|---|---|---|
| MinHook | Native hooks | `FETCHCONTENT_SOURCE_DIR_HKSR_MINHOOK` |
| nlohmann/json | Component serialization | `FETCHCONTENT_SOURCE_DIR_HKSR_JSON` |

Sources have no local patches. Licenses are copied into the development package. To upgrade, change the manifest and run CTest.
