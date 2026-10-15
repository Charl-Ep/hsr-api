#include <hksr/managed/ManagedArrays.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
ArrayMethods ResolveArrayMethods(MethodResolver& source) {
    detail::ResolveBatch b(source);
    ArrayMethods m{
        b.Resolve("System","Array","CreateInstance","System.Array",{"System.Type","System.Int32"},MethodKind::Static),
        b.Resolve("System","Array","get_Length","System.Int32",{}),
        b.Resolve("System","Array","GetValue","System.Object",{"System.Int32"}),
        b.Resolve("System","Array","SetValue","System.Void",{"System.Object","System.Int32"})
    }; m.binding_errors=b.Errors(); return m;
}
HierarchyMethods ResolveHierarchyMethods(MethodResolver& source) {
    return {source.Resolve("UnityEngine","Transform","Find","UnityEngine.Transform",{"System.String"})};
}
}
