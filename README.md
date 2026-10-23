# HksrApi

Windows x64 C++ SDK for Honkai: Star Rail. Version 4.6.2 targets game 4.6.0 and exposes interface generation 1 through `Hksr.Api.dll`.

The SDK provides runtime subscriptions, NPC and independent character creation, model and controller transitions, animation inputs, effects, placement, gaze and character catalogs.

## Development

- [Build and install](BUILDING.md)
- [C++ interface and compatibility](docs/cpp-interface.md)
- [Modules](docs/modules.md)
- [Runtime callbacks](docs/runtime-host.md)
- [Character and resource lifetimes](docs/presentation-lifecycle.md)
- [Animation](docs/animation.md)
- [Animation movement](docs/movement.md)
- [Native support](docs/native-support.md)
- [Contributing](CONTRIBUTING.md)

Consumers link `Hksr::Api` from the SDK development package. Game operations run on the owning official frame. Resources require explicit cleanup; DLL hot-unloading is unsupported.

## License

[WTFPL v2](LICENSE). Dependency licenses are included in the development package.
