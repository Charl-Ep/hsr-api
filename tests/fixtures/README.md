# Test fixtures

- `unity-asset-methods.tsv` contains Unity type, method, return type, parameter and flag declarations, including competing overloads. Tests substitute local function addresses.
- `MeshWriterLocatorTests.cpp` constructs synthetic instruction relationships in memory for relocation, ambiguity and bounds tests.

Fixtures use synthetic declarations and local functions. Tests run independently of the game installation.
