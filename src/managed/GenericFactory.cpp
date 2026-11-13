#include <hksr/managed/GenericFactory.hpp>
#include <hksr/native/Invocation.hpp>
#include <stdexcept>

namespace hksr::api {
struct GenericFactory::State {
    GenericFactoryMethods methods;
    ManagedObjects& objects;
    ManagedArrays arrays;
    ManagedRoots roots;
    std::vector<ManagedRootId> handles;
    void* factory = nullptr;
    void* declaring = nullptr;
    void* argument = nullptr;
    bool attempted = false, closing = false;
    unsigned operations = 0;
    std::string error, name;
    struct Operation {
        State& state;
        explicit Operation(State& value) : state(value) {
            ++state.operations;
        }
        ~Operation() {
            --state.operations;
        }
    };
    State(GenericFactoryMethods m, ReferenceMethods r, ManagedObjects& o, ManagedArrays a)
        : methods(m), objects(o), arrays(std::move(a)), roots(r) {}
    void CheckOpen() const {
        if (closing)
            throw std::logic_error("generic factory closing");
    }
    void* Keep(void* object) {
        CheckOpen();
        if (!object)
            throw std::runtime_error("generic factory reference absent");
        handles.reserve(handles.size() + 1);
        handles.push_back(roots.Keep(object));
        CheckOpen();
        if (!roots.Status(handles.back()).ready)
            throw std::runtime_error("generic factory reference retention failed");
        return object;
    }
    void* Keep(ObjectResult value) {
        if (!value.call.returned)
            throw std::runtime_error("generic factory reflection call failed");
        return Keep(value.object);
    }
};
GenericFactory::GenericFactory(GenericFactoryMethods m, ReferenceMethods r, ManagedObjects& o, ManagedArrays a)
    : state_(new State(m, r, o, std::move(a))) {}
GenericFactory::~GenericFactory() {
    delete state_;
}
bool GenericFactory::Prepare(void* declaring, const char* name, void* argument) {
    auto& s = *state_;
    if (s.attempted || s.closing)
        return s.factory && !s.closing && s.declaring == declaring && s.argument == argument && name && s.name == name;
    s.attempted = true;
    State::Operation operation(s);
    try {
        if (!name || !*name)
            throw std::invalid_argument("generic factory name absent");
        s.declaring = declaring;
        s.argument = argument;
        s.name = name;
        s.Keep(declaring);
        s.Keep(argument);
        auto* text = s.Keep(s.objects.NewStringUtf8(name));
        ObjectResult definition;
        definition.call = native::Call(s.methods.find, &definition.object, declaring, text);
        auto* method = s.Keep(definition);
        bool is_definition = false;
        const auto definition_read = native::Call(s.methods.definition, &is_definition, method);
        s.CheckOpen();
        if (!definition_read.returned || !is_definition)
            throw std::runtime_error("factory is not a generic method definition");
        ObjectResult parameters;
        parameters.call = native::Call(s.methods.arguments, &parameters.object, method);
        auto* arguments = s.Keep(parameters);
        const auto count = s.arrays.Length(arguments);
        s.CheckOpen();
        if (!count.read.returned || count.value != 1)
            throw std::runtime_error("factory requires exactly one generic argument");
        auto* type = s.Keep(s.objects.ManagedType("System", "Type"));
        auto* input = s.Keep(s.arrays.Create(type, 1));
        const auto assigned = s.arrays.Set(input, argument, 0);
        s.CheckOpen();
        if (!assigned.returned)
            throw std::runtime_error("generic argument assignment failed");
        ObjectResult result;
        result.call = native::Call(s.methods.close, &result.object, method, input);
        auto* factory = s.Keep(result);
        bool open = true;
        const auto open_read = native::Call(s.methods.open, &open, factory);
        s.CheckOpen();
        if (!open_read.returned || open)
            throw std::runtime_error("factory still has open generic parameters");
        s.factory = factory;
        return true;
    } catch (const std::exception& e) {
        s.error = e.what();
        return false;
    }
}
ObjectResult GenericFactory::Invoke(void* receiver) const {
    auto& s = *state_;
    ObjectResult out;
    if (!s.closing && s.factory && receiver) {
        State::Operation operation(s);
        out.call = native::Call(s.methods.invoke, &out.object, s.factory, receiver, static_cast<void*>(nullptr));
    }
    return out;
}
bool GenericFactory::Close() {
    auto& s = *state_;
    s.closing = true;
    s.factory = nullptr;
    if (s.operations)
        return false;
    State::Operation operation(s);
    for (auto it = s.handles.begin(); it != s.handles.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.handles.erase(it);
        } else
            ++it;
    }
    return s.handles.empty();
}
const std::string& GenericFactory::Error() const {
    return state_->error;
}
}
