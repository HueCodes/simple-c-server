# Simple C HTTP Server

A production-ready, multithreaded HTTP server written in C99. Designed for learning systems programming, HTTP protocols, and concurrent server design.

## Features

- **Multithreaded**: POSIX threads (pthreads) handle concurrent connections with detached threads
- **Static File Serving**: Serves files from configurable document root (./public by default)
- **Dynamic Routing**: Clean routing table with function pointers for custom handlers
- **Query String Parsing**: URL-decoded query parameters accessible to route handlers
- **MIME Type Detection**: Automatic Content-Type headers based on file extension
- **Directory Index**: Automatic index.html fallback for directory requests
- **Security**: Path traversal protection, input validation, safe string handling
- **Error Handling**: Comprehensive error responses with proper HTTP status codes
- **Graceful Shutdown**: Signal handling for clean resource cleanup (Ctrl+C)
- **Resource Safety**: All memory and file descriptors properly managed

## Supported Routes

Dynamic routes (take precedence over static files):
- `GET /` - Welcome page
- `GET /about` - About page
- `GET /health` - Health check endpoint (JSON)

All other paths serve static files from the document root.

## Building and Running

```bash
# Compile the server
make

# Run on default port 8080
./server

# Run on custom port
./server 3000

# Clean build artifacts
make clean
```

## Testing

```bash
# Start the server
./server

# Test dynamic routes
curl http://localhost:8080/
curl http://localhost:8080/about
curl http://localhost:8080/health

# Test query strings
curl "http://localhost:8080/?name=test&id=123"

# Create public directory and test static files
mkdir -p public
echo "<h1>Static File</h1>" > public/test.html
curl http://localhost:8080/test.html
```

## Project Structure

```
simple-c-server/
├── server.c        # Complete server implementation (~600 lines)
├── Makefile        # Build configuration
├── README.md       # Documentation
└── public/         # Static file document root (create as needed)
```

## Code Architecture

### Route Table System
Routes are defined declaratively using a struct array mapping paths to handler functions:

```c
static const route_t routes[] = {
    {"/", handle_home},
    {"/about", handle_about},
    {NULL, NULL}
};
```

### Request Flow
1. Accept connection and spawn detached pthread
2. Parse HTTP request line (method, path, query string)
3. URL-decode query parameters
4. Check dynamic routes first
5. Fall back to static file serving
6. Send response with proper headers
7. Clean up and close connection

### Safety Features
- strncpy/snprintf used throughout (no unsafe string functions)
- Buffer overflow protection on all inputs
- Path traversal prevention (.. sequences rejected)
- Resource cleanup on all error paths
- Signal-safe shutdown handling

## Design Principles

- **Simplicity**: Single-file implementation under 650 lines
- **Correctness**: Proper error handling and resource management
- **Performance**: Multithreaded design for concurrent requests
- **Portability**: POSIX-compliant, works on Linux/macOS
- **Security**: Input validation and safe string handling
- **Readability**: Clear structure with descriptive names

## Configuration

Modify these constants in server.c:

```c
#define DEFAULT_PORT 8080           // Server port
#define BUFFER_SIZE 8192            // Request buffer size
#define MAX_QUERY_PARAMS 32         // Max query parameters
#define LISTEN_BACKLOG 128          // Connection queue size
#define DOCUMENT_ROOT "./public"    // Static file directory
#define DEFAULT_INDEX "index.html"  // Directory index file
```

## MIME Types

Supported file types:
- HTML/CSS/JavaScript
- JSON/XML
- Images (PNG, JPEG, GIF, SVG, ICO)
- Text/PDF

Add more in the `mime_types` array.

## Requirements

- C99-compatible compiler (GCC or Clang)
- POSIX-compliant system (Linux, macOS, BSD)
- pthread library

## Limitations

- HTTP/1.1 GET requests only
- No HTTPS/TLS support
- No request body parsing (POST/PUT)
- No chunked transfer encoding
- Single file upload not supported
- No keep-alive connections
- No HTTP/2 or HTTP/3

## License

Original work by HueCodes. Free to use for educational purposes.
