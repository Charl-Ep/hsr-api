#pragma once
#include "MetadataResolver.hpp"
#include "../managed/References.hpp"
#include "../managed/ManagedObjects.hpp"
#include "../runtime/GameVersion.hpp"
#include <memory>

namespace hksr::sdk_v1 {
class RuntimeMetadata;
struct RuntimeTypeName { std::string name_space,name; };
// Discovery only: an observed code pointer is not a callable/qualified Method.
struct RuntimeCodeEntries {
    std::vector<void*> codes;
    std::vector<std::string> errors;
};
// Domain-local identity obtained from this provider, never from an offline
// address or a display name. Valid only while that provider/domain is alive.
class RuntimeType {
public:
    RuntimeType() = default;
private:
    friend class RuntimeMetadata;
    RuntimeType(void* klass, const RuntimeMetadata* provider) : klass_(klass), provider_(provider) {}
    void* klass_ = nullptr;
    const RuntimeMetadata* provider_ = nullptr;
};
struct DeclaredField {
    std::string owner_namespace, owner_name, name, type;
    std::size_t offset = 0;
    std::uint32_t flags = 0;
    bool flags_observed = false;
};
// Current-domain instance declarations, including constructed generic types.
// The caller retains the object while querying. No scene inventory is needed.
struct HKSR_API ObjectMetadata {
    virtual ~ObjectMetadata()=default;
    virtual DeclaredType ReadObjectType(void* object)=0;
    virtual std::vector<DeclaredField> ReadObjectFields(void* object)=0;
    // Name-only discovery must not enumerate every method on a task type.
    // Fallback keeps alternate/test providers usable; native provider overrides.
    virtual RuntimeTypeName ReadObjectTypeName(void* object) {
        const auto type=ReadObjectType(object);return {type.name_space,type.name};
    }
};
struct HKSR_API TypeRelationships {
    virtual ~TypeRelationships()=default;
    virtual bool TypeDerivesFrom(const char* name_space,const char* name,
        const char* base_space,const char* base_name)=0;
};
// Reads the current process only. No file-backed report pointers, hooks,
// LoadLibrary, worker thread or game startup are involved.
// Call from an engine-attached thread while the engine domain is alive.
// Caches publish immutable copied values under short locks; native metadata
// reads occur outside those locks. This protects cache memory, not arbitrary
// game-object operations or the domain's lifetime.
class HKSR_API RuntimeMetadata final : public MethodResolver, public ManagedObjects, public ObjectMetadata, public TypeRelationships {
public:
    // Metadata/managed-primitives qualification only. Does not enable the
    // actor host or certify every Unity value/native-resource contract.
    static std::unique_ptr<RuntimeMetadata> Open(RuntimeVersion version);
    ~RuntimeMetadata();
    RuntimeMetadata(const RuntimeMetadata&) = delete;
    RuntimeMetadata& operator=(const RuntimeMetadata&) = delete;
    Method Resolve(const char* name_space, const char* type, const char* name,
        const char* result, std::initializer_list<const char*> parameters,
        MethodKind kind = MethodKind::Instance) override;
    DeclaredType ReadType(const char* name_space, const char* name);
    std::vector<DeclaredField> ReadFields(const char* name_space, const char* name, bool include_parents = true);
    std::size_t TypeCount() const;
    // Copies names from the already captured domain inventory. No method/field
    // traversal, game calls, or new assembly scan; duplicate names are retained.
    std::vector<RuntimeTypeName> TypeNames() const;
    // Indexes refer to this provider's captured domain inventory, including
    // duplicate display names. No new assembly scan or target method execution.
    RuntimeCodeEntries ReadCodeEntries(std::size_t type_index) const;
    DeclaredType ReadTypeAt(std::size_t type_index);
    RuntimeVersion Version() const;
    std::vector<std::string> UnavailableOperations() const;
    // Returned managed objects are NOT rooted. Retain before storing across
    // calls/frames. AllocateObject does not invoke a constructor, nor does it
    // create a Unity native object. Type lookup uses the same unique identity
    // rule as method lookup; it never falls back to another named class.
    ObjectResult NewStringUtf8(const char* text) override;
    ObjectResult AllocateObject(const char* name_space, const char* name) override;
    ObjectResult ManagedType(const char* name_space, const char* name) override;
    RuntimeType FindType(const char* name_space, const char* name);
    RuntimeType ParameterType(Method method, unsigned index);
    RuntimeType ReturnType(Method method);
    RuntimeType ObjectType(void* object);
    DeclaredType ReadObjectType(void* object) override;
    RuntimeTypeName ReadObjectTypeName(void* object) override;
    std::vector<DeclaredField> ReadObjectFields(void* object) override;
    bool TypeDerivesFrom(const char* name_space,const char* name,const char* base_space,const char* base_name) override;
    DeclaredType ReadType(RuntimeType type);
    std::vector<DeclaredField> ReadFields(RuntimeType type, bool include_parents = true);
    Method Resolve(RuntimeType type, const char* name, const char* result,
        std::initializer_list<const char*> parameters, MethodKind kind = MethodKind::Instance);
    ObjectResult AllocateObject(RuntimeType type);
    ObjectResult ManagedType(RuntimeType type);
private:
    void* CheckedClass(RuntimeType type) const;
    struct Storage;
    explicit RuntimeMetadata(Storage* storage);
    Storage* storage_;
};
}
