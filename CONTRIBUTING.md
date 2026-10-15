# Contributing

Start with [BUILDING](BUILDING.md), [modules](docs/modules.md) and the [C++ contract](docs/cpp-interface.md).

- Add public interfaces under `include/hksr/<module>`, implementations under `src/<module>`, and tests under the corresponding test directory.
- Put game declarations and version-specific calling layouts in `adapters/<module>`.
- Update `cmake/Sources.cmake` when adding or moving source files.
- For ownership changes, test cancellation, reentry, delayed callbacks, partial cleanup and entity-address reuse.
- Preserve public value layouts, signatures and ownership within an interface generation. Incompatible changes require a new generation.
- Run CTest and package-consumer checks. Describe the affected interface and test results in the change.
- Keep game assets, binaries, local logs and credentials out of the repository.

Dependency revisions are managed in [dependencies.json](dependencies.json). Contributions use [WTFPL v2](LICENSE).
