# Server Tools Documentation (incomplete)

The code in this project is heavily commented and the header files could (and probably should) be used for the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is well... less helpful and harder to navigate (I'll leave it at that for now).

The documentation in this folder includes:

* The main [`libserver` API documentation](libserver.md).

    This documents the main library API and should be used when writing custom protocols. This API is (mostly) redundant when using the `http` or `websockets` protocol extensions.

* The [`http` extension API documentation](http.md) (coming soon).

    The `http` protocol extension allows quick access to the HTTP protocol necessary for writing web applications.

    Although the `libserver` API is still accessible, the `struct HttpRequest` and `struct HttpResponse` objects and API provide abstractions for the raw HTTP protocol and should be preferred.

* The [`websockets` extension API documentation](websockets.md) (coming soon).

    The `websockets` protocol extension allows quick access to the HTTP and Websockets protocols necessary for writing real-time web applications.

    Although the `libserver` API is still accessible, the `struct HttpRequest` and `struct HttpResponse` objects and API provide abstractions for the raw HTTP protocol and should be preferred.

* Core documentation that documents the libraries used internally.

    The core documentation can be safely ignored by anyone using the `libserver`, `http` or `websockets` frameworks.

    The core libraries include (coming soon):

    * [`libreact`](./libreact.md) - The reactor core functionality (EPoll and KQueue abstractions).

    * [`libasync`](./libasync.md) - The thread pool and task management core functionality.

    * [`libsock`](./libsock.md) - A sockets library that resolves common issues such as fd collisions and user land buffer.

    * [`minicrypt`](./minicrypt.md) - Cryptography and Base64 encoding helpers (used during the `websockets` handshake) (coming soon).
