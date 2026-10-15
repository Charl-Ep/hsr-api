# Native support

`HksrApiNative` provides a static library for adapters and in-process diagnostic tools.

```cmake
find_package(HksrApiNative 4.6.2 EXACT CONFIG REQUIRED)
target_link_libraries(MyDiagnostic PRIVATE Hksr::NativeSupport)
```

Headers install under `include/HksrApiNative` and are included as `<hksr/native/...>`. Use the exact SDK package and its [C++ build settings](cpp-interface.md).

| Header | Function |
|---|---|
| `MetadataReader.hpp` | Read a supplied API table and metadata layout, with per-entry errors |
| `ObjectAccess.hpp` | Invoke native string, allocation and reflection-type primitives |
| `LoadedModule.hpp` | Inspect an already loaded PE image |
| `UnityMetadataLocator.hpp` | Locate the root-domain table relationship in supplied bytes |
| `Invocation.hpp` | Typed managed invocation with trailing MethodInfo and an SEH result |

The caller supplies the ABI, valid thread/domain and object lifetime. Native object primitives have no trailing MethodInfo; managed methods do. A failed invocation may have partially changed state and is not automatically safe to retry.

`ObserveTypes`, `ObserveMethods` and `ObserveFields` return partial metadata observations. `ReadMethods` additionally uses a method layout and executable-address predicate. Type-name buffers are released with their paired API-table operation.

The library has no shared runtime state. Runtime selection, hooks, thread attachment and managed ownership belong to `Hksr.Api`. Local tests are provided by `sdk.native-support`.
