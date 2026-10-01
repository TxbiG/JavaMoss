# Contributing to JavaMoss

Thank you for contributing to **JavaMoss**, the JNI binding layer for Moss Framework.

JavaMoss exposes Moss through a Java-facing API while keeping C++ implementation details behind the native boundary. Correct ownership, JNI lifetime management, ABI stability, and predictable Java behaviour are therefore important parts of every change.

## Before You Start

For changes involving JNI ownership, native ABI, public Java APIs, or binding architecture, open an issue before substantial implementation.

Please search existing issues and pull requests before starting work.

## Development Requirements

- Git
- A suitable JDK
- CMake
- C++17
- JNI development headers/toolchain
- A local Moss Framework checkout

## Building

```bash
git clone https://github.com/TxbiG/JavaMoss.git
cd JavaMoss

cmake -S . -B build
cmake --build build
```

Use the repository's existing Java/native configuration for packaging and runtime tests.

## Repository Structure

- `src/main/java/dev/moss/` — Java API
- `native/` — native/JNI implementation
- `docs/` — documentation
- `examples/` — examples
- `performance/` — benchmarks
- `build/workflows/` — CI/build configuration

## Areas for Contribution

- Java API coverage
- JNI implementation
- Resource ownership/lifetime
- Exception handling
- Native/Java conversion
- Performance
- Packaging
- Examples
- Documentation
- Tests
- Cross-platform native loading

## JNI Guidelines

Keep the JNI boundary explicit and as small as practical.

Java-facing code should not expose C++ object layouts or backend-specific implementation details.

Pay particular attention to:

- Native handle lifetime
- JNI local/global references
- Thread attachment
- Exception propagation
- String and array conversions
- Native memory cleanup
- Calling conventions and ABI

## API Design

Java APIs should follow normal Java conventions while preserving Moss semantics.

Document non-obvious:

- Construction rules
- Ownership
- Lifetime
- Threading expectations
- Exceptions
- Resource cleanup

Avoid abstractions that make the native resource lifetime ambiguous.

## Testing

Test both Java-side and native-side behaviour.

Important cases include:

- Invalid native handles
- Native resource destruction
- Repeated create/destroy cycles
- JNI exceptions
- String/array conversions
- Native library loading
- Platform-specific behaviour

## Commit Messages

Recommended prefixes:

```text
feat: expose Moss audio devices
fix: release native window correctly
docs: document Java resource lifetime
test: add JNI handle regression
perf: reduce JNI conversion overhead
build: improve native library discovery
```

## Pull Requests

Include:

- Description
- Java usage example where appropriate
- Tests performed
- JDK version
- Operating system
- Native toolchain
- ABI/API impact

## Licence

JavaMoss is distributed under the **MIT License**. Contributions should be compatible with the repository's licence and applicable JNI/Moss/third-party licence requirements.

Thank you for contributing.
