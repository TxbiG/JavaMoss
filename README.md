# JavaMoss

JavaMoss is a JNI binding layer for [MossFramework](https://github.com/TxbiG/Moss). It exposes an
idiomatic, ownership-safe Java API instead of exposing C++ layouts or symbols directly.

## Documentation

Comprehensive documentation is available in the [`docs/`](./docs) directory:

* Architecture overview
* Rendering system design
* Physics system design
* Audio pipeline
* Input system
* Networking model
* Platform backend architecture
* Performance guidelines

- [API cheatsheet](docs/API_Cheatsheet.md)

## Design

Every native resource is an `AutoCloseable` Java owner around an opaque `long` handle.
Native methods are implementation details; public methods validate Java inputs before
crossing JNI. This keeps C++ template types, callbacks, and ownership rules out of the
Java ABI. Callbacks are intentionally deferred because they need a separate lifetime and
thread-attachment design.

## License
The project is distributed under the [MIT license](LICENSE).
