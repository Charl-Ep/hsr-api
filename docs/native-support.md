# Native support

`HksrApiNative` provides a static library for adapters and in-process diagnostic tools.

```cmake
find_package(HksrApiNative 4.7.0 EXACT CONFIG REQUIRED)
target_link_libraries(MyDiagnostic PRIVATE Hksr::NativeSupport)
```

Headers install under `include/HksrApiNative` and are included as `<hksr/native/...>`. Use the exact SDK package and its [C++ build settings](cpp-interface.md).

| Header | Function |
|---|---|
| `MetadataReader.hpp` | Read a supplied API table and metadata layout, with per-entry errors |
| `MetadataProvider.hpp` | Construct RuntimeMetadata from an adapter-qualified connection; also link Hksr::Api |
| `ObjectAccess.hpp` | Invoke native string, allocation and reflection-type primitives |
| `LoadedModule.hpp` | Inspect an already loaded PE image |
| `UnityMetadataLocator.hpp` | Locate the root-domain table relationship in supplied bytes |
| `Invocation.hpp` | Typed managed invocation with trailing MethodInfo and an SEH result |

The caller supplies the ABI, valid thread/domain and object lifetime. Managed methods take a trailing MethodInfo; native object primitives use their declared native signatures. A failed invocation may have partially changed state; establish its outcome before retrying.

`ObserveTypes`, `ObserveMethods` and `ObserveFields` return partial metadata observations. `ObserveDeclaredMethods` additionally applies the supplied method layout and executable-address predicate with per-entry errors; `ReadMethods` requires a complete result. Type-name buffers are released with their paired API-table operation.

`RuntimeMetadata::TypeAt` preserves captured type identity when display names collide. `ParentType` and `ObserveFields` retain constructed generic owners; fields carry handles for `FieldType` and `DeclaringType`. `ObserveMethods` returns partial declarations for an actual type.

`native::MetadataProvider::Open` builds a provider from supplied tables. `FromTypeDescriptor` requires an IL2CPP type descriptor with caller-established provenance and a live domain. Ordinary runtime clients receive the selected provider in `Prepare`.

`Hksr.Api` owns shared runtime selection, hooks, thread attachment and managed ownership. NativeSupport is a standalone static library. Local tests are provided by `sdk.native-support`.
