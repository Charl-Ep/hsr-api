#include <hksr/managed/GenericFactory.hpp>
namespace hksr::sdk_v1 {
GenericFactoryMethods ResolveGenericFactoryMethods(MethodResolver& r) {
    return {
        r.Resolve("System","Type","GetMethod","System.Reflection.MethodInfo",{"System.String"}),
        r.Resolve("System.Reflection","MonoMethod","get_IsGenericMethodDefinition","System.Boolean",{}),
        r.Resolve("System.Reflection","MonoMethod","GetGenericArguments","System.Type[]",{}),
        r.Resolve("System.Reflection","MonoMethod","MakeGenericMethod","System.Reflection.MethodInfo",{"System.Type[]"}),
        r.Resolve("System.Reflection","MonoMethod","get_ContainsGenericParameters","System.Boolean",{}),
        r.Resolve("System.Reflection","MethodBase","Invoke","System.Object",{"System.Object","System.Object[]"})};
}
}
