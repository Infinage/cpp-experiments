# Networking

This directory contains C++ projects showcasing basic networking concepts.

## Projects

### 1. `hello-socket.cpp`
A minimalistic socket program that can operate as both a server and a client based on input arguments.

#### Features
- **Server Mode**:
  - Creates a socket and binds it to an IP and port.
  - Listens for incoming connections and handles them in separate threads.
  - Receives data from clients and responds with a message.
  - Handles multiple connections *gracefully*.

- **Client Mode**:
  - Connects to the server using its IP and port.
  - Sends a message to the server and receives a response.

#### Usage
Compile the program:
```bash
g++ hello-socket.cpp -o hello-socket -std=c++23
```
Run as a server:
```bash
./hello-socket server <IPv4> <port>
```
Run as a client:
```bash
./hello-socket client <IPv4> <port>
```

### 2. `http-server.cpp`
A basic HTTP server program that serves static files from a specified directory.

#### Features
- Serves files from a user-specified directory over HTTP.
- Responds to basic HTTP GET requests with the appropriate file contents.

#### Usage
Compile the program:
```bash
g++ http-server.cpp ../misc/ThreadPool.cpp -o http-server -std=c++23
```
Run the server:
```bash
./http-server <port> <path>
```
- `<port>`: Port number to bind the server to.
- `<path>`: Path to the directory containing files to be served.

## Prerequisites
- C++23 compiler (e.g., `g++` with C++23 support).
- Code is tested to work on Ubuntu WSL. Windows is currently not supported.

## Notes
- These projects are intended for educational purposes.
- Error handling is basic and can be extended as needed.
- Ensure you have the necessary permissions to bind to the specified ports and access the file paths.
