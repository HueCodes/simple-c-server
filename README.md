# Simple C HTTP Server

A simple, educational HTTP server written in C. Great for learning C programming and basic HTTP protocol concepts.

## Features

- **Route Handling**: Supports multiple endpoints (/, /about, /health)
- **HTTP Method Support**: Handles GET requests properly
- **Error Responses**: Proper HTTP error codes (400, 404, 405)
- **Graceful Shutdown**: Signal handling for clean shutdown (Ctrl+C)
- **Socket Reuse**: Prevents "Address already in use" errors
- **Request Parsing**: Simple HTTP request parser without recursion
- **Structured Responses**: Proper HTTP headers and content types

## Supported Routes

- `GET /` - Welcome page with HTML content
- `GET /about` - About page with server information
- `GET /health` - Health check endpoint (JSON response)

## Building and Running

```bash
# Compile the server
make

# Run the server
./server

# Clean build files
make clean
```

The server listens on port 8080. You can test it by visiting:
- http://localhost:8080/
- http://localhost:8080/about
- http://localhost:8080/health

## Design Principles

- **Simplicity**: Keep the code simple and readable
- **No Recursion**: All algorithms use iterative approaches
- **Linear Complexity**: Route matching uses simple linear search
- **Error Handling**: Comprehensive error checking and reporting
- **Memory Safety**: Careful buffer management and bounds checking

